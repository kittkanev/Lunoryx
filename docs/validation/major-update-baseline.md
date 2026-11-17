# Stage 0 baseline validation

Baseline source: 1.3.1, `2212256ec5537f1fb850a1bb8298c8d323f0820b`. Target approved by owner: 1.4.0; new project schema with migration/backup; external local HMAC key. No new dependencies downloaded or introduced.

## Actual Linux commands and results

Environment: existing `/workspace/.lunoryx-env/activate.sh`, pinned cached Rust 1.85.1, existing self-contained C# worker. A dry run was inspected before execution; no dependency setup/download commands were needed; Cargo used `--locked --offline`.

- `make -j2 test test-integration test-physics test-rust`: **pass**, including real C# compile/modules/messages and all 1.3.1 integration cases.
- `make test-postprocess test-render`: **pass**, software Mesa/SDL offscreen; resource lifecycle and cached/reference parity.
- `python tools/test_release_safety.py`: **pass**, eight cases.
- `python tools/build_manual.py`: **pass**, EN/RU plus embedded help generated. Register schema/ID checks: **pass**, 61 IDs in both languages, 40 original audit entries and nine 1.3.1 regression groups retained.

Selected actual output:

```text
test result: ok. 6 passed; 0 failed; 0 ignored; 0 measured; 0 filtered out; finished in 0.58s
PASS: C11 animation interpolation, shortest rotation, easing, loop and endpoint
PASS: five mesh types, bounds, winding, quaternion, duplication
PASS: polygon topology, modifier base preservation, closed bevel, extrude, Loop subdivision, CSG volumes and invalid input
PASS: 3840 physics steps and 2000 bounds fixtures match 1.0.1 bit-for-bit
PASS: animation/project compatibility, Lua API, swept killzones, checkpoint respawn, non-solid triggers, traceback and Play restore
PASS: free camera eye invariance, six-axis normalized flight, box/circle geometry and Object/Edit vertex-edge-face modes
PASS: Play save protection and transactional rejection of malformed project numbers
PASS: cached indexed material batches, flat normals, exact edit invalidation and cache lifetime
PASS: geometry node evaluation, cycles/budgets, project round trip, undo/redo, bake and sculpt
PASS: build settings and GUI persistence/undo, transactional validation, Lua GUI, swept trigger enter/stay/exit and callback traceback
PASS: real C# compilation/modules, Lua messages, stable IDs, diagnostics and deadline cancellation
PASS: dense triggers, thin-wall CCD, directory projects, disk modules and Play recovery
PASS: 1.3.1 Lua bytes, signed-zero dirty state, audio Stop cleanup, sparse >3 GiB package and invalid offset
PASS: Lua bounds, stack recovery, hostile metamethods, GLB buffer round trip
```

## Local GUI cache diagnosis

The initial saved engine/testing executables dated before 1.3.1; GUI/packaged-game execution hit SIGILL. Rebuilding current source (`make -j1 engine engine-testing`) did not resolve SIGILL with the persisted Mesa shader cache. strace located the fault in a graphics worker; the identical fresh executable passed the UI smoke when `MESA_SHADER_CACHE_DISABLE=true` was set. This isolates a cache-dependent local graphics failure; a specific driver/cache root cause has not been proved. No production workaround or unsupported hardware claim was introduced.

- `MESA_SHADER_CACHE_DISABLE=true SDL_VIDEODRIVER=offscreen LIBGL_ALWAYS_SOFTWARE=1 TEST_TIMEOUT=2 LUNORYX_UI_SMOKE=1 ./engine-testing`: **pass**.
- `MESA_SHADER_CACHE_DISABLE=true LUNORYX_TEST_RUNTIME="$PWD/engine-testing" LUNORYX_GAME_GUI=1 LUNORYX_GAME_SMOKE=1 SDL_VIDEODRIVER=offscreen LIBGL_ALWAYS_SOFTWARE=1 python tools/test_game_package.py ./engine --managed`: **pass** after source rebuild. Initial cache-enabled attempt **failed (SIGILL)**. Actual final output: `PASS: third-person input, physics, all pickups, score, embedded audio, HUD and restart`; `PASS: native single-executable packer, UTF-8 paths, relocation, removed source, corrupted payload, overlap and nesting rejection`.

## Native and long-run evidence

- [Complete native 1.3.1 CI](https://github.com/kittkanev/Lunoryx/actions/runs/43246269279): successful, exact baseline SHA. This is historical baseline evidence, not validation of future 1.4.0 features.
- [Scheduled one-hour project-parser campaign](https://github.com/kittkanev/Lunoryx/actions/runs/43703840148): successful on the same baseline SHA. This does not demonstrate GPU, audio, GLB or coverage-guided fuzz completeness.
- Fresh Windows/macOS, a full sanitizer rebuild, optional certificate/notarization execution and 1.4.0 UI video are **not run in this Stage 0 Linux pass**. They remain release gates.
