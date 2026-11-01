//! Allocation-free kernels. Only validated buffers cross the C ABI.
#![no_std]

#[cfg(not(test))]
#[panic_handler]
fn panic(_: &core::panic::PanicInfo<'_>) -> ! {
    extern "C" {
        fn abort() -> !;
    }
    // No unwinding across C++; an internal invariant failure terminates.
    unsafe { abort() }
}

#[cfg(any(test, feature = "bench"))]
const fn crc_tables() -> [[u32; 256]; 8] {
    let mut tables = [[0; 256]; 8];
    let mut i = 0;
    while i < 256 {
        let mut c = i as u32;
        let mut j = 0;
        while j < 8 {
            c = (c >> 1) ^ (0xedb88320u32 & 0u32.wrapping_sub(c & 1));
            j += 1;
        }
        tables[0][i] = c;
        i += 1;
    }
    let mut t = 1;
    while t < 8 {
        i = 0;
        while i < 256 {
            let c = tables[t - 1][i];
            tables[t][i] = (c >> 8) ^ tables[0][(c & 255) as usize];
            i += 1;
        }
        t += 1;
    }
    tables
}
#[cfg(any(test, feature = "bench"))]
static CRC: [[u32; 256]; 8] = crc_tables();

#[cfg(any(test, feature = "bench"))]
pub fn crc32(bytes: &[u8]) -> u32 {
    let mut crc = !0u32;
    let mut chunks = bytes.chunks_exact(8);
    for chunk in &mut chunks {
        let a = crc ^ u32::from_le_bytes([chunk[0], chunk[1], chunk[2], chunk[3]]);
        crc = CRC[7][(a & 255) as usize]
            ^ CRC[6][((a >> 8) & 255) as usize]
            ^ CRC[5][((a >> 16) & 255) as usize]
            ^ CRC[4][(a >> 24) as usize]
            ^ CRC[3][chunk[4] as usize]
            ^ CRC[2][chunk[5] as usize]
            ^ CRC[1][chunk[6] as usize]
            ^ CRC[0][chunk[7] as usize];
    }
    for &byte in chunks.remainder() {
        crc = (crc >> 8) ^ CRC[0][((crc ^ byte as u32) & 255) as usize];
    }
    !crc
}

/// # Safety
/// `data` must cover `size` readable bytes (null is allowed only when size=0).
#[cfg(feature = "bench")]
#[no_mangle]
pub unsafe extern "C" fn lunoryx_crc32(data: *const u8, size: usize) -> u32 {
    if size == 0 {
        return 0;
    }
    crc32(core::slice::from_raw_parts(data, size))
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct WeldCell {
    pub x: i64,
    pub y: i64,
    pub z: i64,
    pub ordinal: u32,
}

fn coordinate(value: f32, tolerance: f32) -> Option<i64> {
    let ratio = value as f64 / (tolerance as f64 * 2.0);
    // Leave headroom for neighbours, and avoid lossy integer cell boundaries.
    if !ratio.is_finite() || ratio.abs() > (1u64 << 50) as f64 {
        return None;
    }
    let truncated = ratio as i64;
    Some(if ratio < truncated as f64 {
        truncated - 1
    } else {
        truncated
    })
}

fn cell(positions: &[f32], index: u32, tolerance: f32) -> Option<(i64, i64, i64)> {
    let i = index as usize * 3;
    Some((
        coordinate(positions[i], tolerance)?,
        coordinate(positions[i + 1], tolerance)?,
        coordinate(positions[i + 2], tolerance)?,
    ))
}

fn key(c: &WeldCell) -> (i64, i64, i64) {
    (c.x, c.y, c.z)
}

// Return 2 when the numeric range requires the unchanged C++ reference path.
fn weld(
    positions: &[f32],
    selection: &[u32],
    tolerance: f32,
    workspace: &mut [WeldCell],
    output: &mut [u32],
) -> i32 {
    if positions.len() % 3 != 0
        || selection.len() < 2
        || selection.len() > 200000
        || workspace.len() != selection.len()
        || output.len() != selection.len()
        || !tolerance.is_finite()
        || tolerance <= 0.0
    {
        return 1;
    }
    for (ordinal, &index) in selection.iter().enumerate() {
        if index as usize >= positions.len() / 3 || (ordinal > 0 && selection[ordinal - 1] >= index)
        {
            return 1;
        }
        let Some((x, y, z)) = cell(positions, index, tolerance) else {
            return 2;
        };
        workspace[ordinal] = WeldCell {
            x,
            y,
            z,
            ordinal: ordinal as u32,
        };
    }
    workspace.sort_unstable_by_key(|c| (c.x, c.y, c.z, c.ordinal));
    let limit = tolerance as f64 * tolerance as f64;
    for (ordinal, &index) in selection.iter().enumerate() {
        let (x, y, z) = cell(positions, index, tolerance).unwrap();
        let mut best = ordinal;
        // Cells are twice the tolerance, covering the original float-distance
        // rounding at the threshold without widening the neighbour search.
        'neighbours: for dx in -1..=1 {
            for dy in -1..=1 {
                for dz in -1..=1 {
                    let wanted = (x + dx, y + dy, z + dz);
                    let mut at = workspace.partition_point(|c| key(c) < wanted);
                    while at < workspace.len() && key(&workspace[at]) == wanted {
                        let previous = workspace[at].ordinal as usize;
                        if previous >= best {
                            break;
                        }
                        let a = index as usize * 3;
                        let b = selection[previous] as usize * 3;
                        // Match C++ float subtraction and dot-product order exactly.
                        let px = positions[a] - positions[b];
                        let py = positions[a + 1] - positions[b + 1];
                        let pz = positions[a + 2] - positions[b + 2];
                        let distance =
                            (px as f64 * px as f64 + py as f64 * py as f64) + pz as f64 * pz as f64;
                        if distance <= limit {
                            best = previous;
                            if best == 0 {
                                break 'neighbours;
                            }
                        }
                        at += 1;
                    }
                }
            }
        }
        output[ordinal] = if best == ordinal { index } else { output[best] };
    }
    0
}

/// # Safety
/// All buffers must be aligned and valid for their specified lengths. Mutable
/// workspace/output must not overlap each other or either input. No allocation
/// or ownership passes through this ABI. Maximum selection/vertex count: 200000.
#[no_mangle]
pub unsafe extern "C" fn lunoryx_weld_map(
    positions: *const f32,
    vertex_count: usize,
    selection: *const u32,
    count: usize,
    tolerance: f32,
    workspace: *mut WeldCell,
    output: *mut u32,
) -> i32 {
    if vertex_count > 200000
        || count < 2
        || count > 200000
        || positions.is_null()
        || selection.is_null()
        || workspace.is_null()
        || output.is_null()
    {
        return 1;
    }
    weld(
        core::slice::from_raw_parts(positions, vertex_count * 3),
        core::slice::from_raw_parts(selection, count),
        tolerance,
        core::slice::from_raw_parts_mut(workspace, count),
        core::slice::from_raw_parts_mut(output, count),
    )
}

#[cfg(feature = "bench")]
#[no_mangle]
pub unsafe extern "C" fn lunoryx_crc32_reference(data: *const u8, size: usize) -> u32 {
    let bytes = core::slice::from_raw_parts(data, size);
    let mut crc = !0u32;
    for &byte in bytes {
        crc ^= byte as u32;
        for _ in 0..8 {
            crc = (crc >> 1) ^ (0xedb88320u32 & 0u32.wrapping_sub(crc & 1));
        }
    }
    !crc
}

#[cfg(feature = "bench")]
#[no_mangle]
pub unsafe extern "C" fn lunoryx_bounds_reference(data: *const f32, count: usize, out: *mut f32) {
    let data = core::slice::from_raw_parts(data, count * 3);
    let out = core::slice::from_raw_parts_mut(out, 6);
    out[..3].copy_from_slice(&data[..3]);
    out[3..].copy_from_slice(&data[..3]);
    for p in data.chunks_exact(3).skip(1) {
        for k in 0..3 {
            out[k] = out[k].min(p[k]);
            out[k + 3] = out[k + 3].max(p[k]);
        }
    }
}

#[cfg(test)]
mod tests;
