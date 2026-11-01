extern crate std;
use super::*;
use std::vec;
use std::vec::Vec;
fn reference(p: &[f32], selected: &[u32], t: f32) -> Vec<u32> {
    let mut out = Vec::new();
    for (ordinal, &index) in selected.iter().enumerate() {
        let mut result = index;
        for (j, &previous) in selected[..ordinal].iter().enumerate() {
            let (a, b) = (index as usize * 3, previous as usize * 3);
            let (x, y, z) = (p[a] - p[b], p[a + 1] - p[b + 1], p[a + 2] - p[b + 2]);
            if (x as f64 * x as f64 + y as f64 * y as f64) + z as f64 * z as f64
                <= t as f64 * t as f64
            {
                result = out[j];
                break;
            }
        }
        out.push(result);
    }
    out
}
fn compare(p: &[f32], selected: &[u32], t: f32) {
    let mut workspace = vec![WeldCell::default(); selected.len()];
    let mut out = vec![u32::MAX; selected.len()];
    let result = weld(p, selected, t, &mut workspace, &mut out);
    assert_eq!(result, 0);
    assert_eq!(out, reference(p, selected, t));
}
#[test]
fn ordered_chain_and_negative_cells() {
    compare(
        &[
            0., 0., 0., 0.9, 0., 0., 1.8, 0., 0., -0.9, 0., 0., -1.8, 0., 0.,
        ],
        &[0, 1, 2, 3, 4],
        1.,
    );
}
#[test]
fn boundaries_random_subsets_and_duplicates() {
    let mut seed = 73u32;
    for run in 0..200 {
        let t = [0.001f32, 0.1, 1., 100.][run % 4];
        let mut p = Vec::new();
        let mut selected = Vec::new();
        for i in 0..200 {
            for _ in 0..3 {
                seed = seed.wrapping_mul(1664525).wrapping_add(1013904223);
                p.push((seed % 31) as f32 * t + (if seed & 32 == 0 { -0.5 } else { 0.5 }) * t);
            }
            if i % 3 != 1 {
                selected.push(i);
            }
        }
        compare(&p, &selected, t);
    }
    for t in [0.001f32, 1., 1000.] {
        for base in [-10000f32, -2., -1., 0., 1., 2., 10000.] {
            let p = [
                base * t,
                0.,
                0.,
                (base + 1.) * t,
                0.,
                0.,
                (base + 2.) * t,
                0.,
                0.,
            ];
            compare(&p, &[0, 1, 2], t);
        }
    }
}
#[test]
fn rejects_invalid_and_falls_back_before_output() {
    let p = [0., 0., 0., 1., 0., 0.];
    let mut work = [WeldCell::default(); 2];
    let mut out = [99; 2];
    assert_eq!(weld(&p, &[1, 0], 1., &mut work, &mut out), 1);
    assert_eq!(weld(&p, &[0, 2], 1., &mut work, &mut out), 1);
    assert_eq!(weld(&p, &[0, 1], f32::NAN, &mut work, &mut out), 1);
    assert_eq!(weld(&p, &[0, 1], f32::MIN_POSITIVE, &mut work, &mut out), 2);
    assert_eq!(out, [99; 2]);
    assert_eq!(
        unsafe {
            lunoryx_weld_map(
                core::ptr::null(),
                2,
                core::ptr::null(),
                2,
                1.,
                core::ptr::null_mut(),
                core::ptr::null_mut(),
            )
        },
        1
    );
}
#[test]
fn crc_candidate_compatibility() {
    assert_eq!(crc32(b"123456789"), 0xcbf43926);
    assert_eq!(crc32(b""), 0);
    let mut data = vec![0u8; 1000];
    let mut seed = 73u32;
    for b in &mut data {
        seed = seed.wrapping_mul(1664525).wrapping_add(1013904223);
        *b = (seed >> 24) as u8;
    }
    for start in 0..8 {
        for end in start..data.len() {
            let mut crc = !0u32;
            for &b in &data[start..end] {
                crc ^= b as u32;
                for _ in 0..8 {
                    crc = (crc >> 1) ^ (0xedb88320u32 & 0u32.wrapping_sub(crc & 1));
                }
            }
            assert_eq!(crc32(&data[start..end]), !crc);
        }
    }
}

#[test]
fn abi_layout() {
    assert_eq!(core::mem::size_of::<WeldCell>(), 32);
    assert_eq!(core::mem::align_of::<WeldCell>(), 8);
}
#[test]
fn next_float_thresholds_and_signed_zero() {
    for tolerance in [0.001f32, 0.5, 1., 1000.] {
        let below = f32::from_bits(tolerance.to_bits() - 1);
        let above = f32::from_bits(tolerance.to_bits() + 1);
        compare(
            &[
                0., 0., 0., tolerance, 0., 0., below, 0., 0., above, 0., 0., -tolerance, 0., 0.,
                -above, 0., 0., -0., 0., 0.,
            ],
            &[0, 1, 2, 3, 4, 5, 6],
            tolerance,
        );
        let edge = tolerance * 2.;
        let before = f32::from_bits(edge.to_bits() - 1);
        let after = f32::from_bits(edge.to_bits() + 1);
        compare(
            &[
                before, 0., 0., after, 0., 0., -before, 0., 0., -after, 0., 0.,
            ],
            &[0, 1, 2, 3],
            tolerance,
        );
    }
}
