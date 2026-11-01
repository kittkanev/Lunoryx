#pragma once
#include <cstddef>
#include <cstdint>
// Valid readable byte buffer; nullptr is allowed only for size == 0.
#ifdef LUNORYX_KERNEL_BENCHMARK
extern "C" uint32_t lunoryx_crc32(const unsigned char *data, size_t size);
#endif
struct LunoryxWeldCell { int64_t X,Y,Z; uint32_t Ordinal; };
// Caller owns aligned, disjoint buffers. Selection is strictly ascending.
// Return 0: success; 1: invalid input; 2: use the C++ numeric-range fallback.
extern "C" int32_t lunoryx_weld_map(const float *positions, size_t vertices,
    const uint32_t *selection, size_t count, float tolerance,
    LunoryxWeldCell *workspace, uint32_t *output);
