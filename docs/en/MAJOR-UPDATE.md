# Lunoryx — Stage 0: major offline update register

Baseline: **1.3.1**, commit `2212256ec5537f1fb850a1bb8298c8d323f0820b`. This register distinguishes implemented development slices from planned features; readiness and evidence are recorded per item.

Sources: [machine-readable register](../validation/major-update-register.json), [original audit](../audit-v1.3.0.json), [1.3.1 fixes](../validation/v1.3.1-fixes.md), [Stage 0 local checks](../validation/major-update-baseline.md).

## Order and boundaries

0. Inventory, source checks, baseline tests and current EN/RU help — this stage.
1. F0: security/crashes/resources → correctness: package trust, budget stress, signing-CI preparation and preserved boundaries.
2. P1: scene/format/API contracts and migrations first; then skeleton/physics, material/render/modeling, saves/input. Each vertical slice includes runtime, editor, persistence, Lua/C# and tests; an isolated math function is not a completed feature.
3. P2: audio/GUI/script/asset/UX and experimental stabilization over working P1.
4. Do not start P3 before agreed P1/P2 completion. Publish a major release only after the agreed scope and full gate.

`tools/check_major_release.py` rejects 1.4.0 publication while any P1/P2 feature is unfinished or lacks local test/validation references. It supplements the exact-commit native/graphics/sanitizer/video gate. Current development is intentionally incomplete.

## Owner decisions — confirmed

- Target version: **1.4.0**. Readiness depends on implementation and validation, not the version string.
- A new project format is **approved**, with migration and a separate backup before first write; preserve original projects and .pre-1.3.bak.
- HMAC is a symmetric MAC, not a public digital signature. A secret embedded in a distributed EXE does not prevent forgery. An external local secret authenticates for key holders; verification by players without a secret requires a different signature model. The owner chose an **external local HMAC key**; new packages require authentication and legacy must not be silently accepted.
- Application/game: no accounts, servers, telemetry, remote asset fetch or new network dependencies. MCP stays local stdio. GitHub CI is allowed; prepare signing/notarization only in CI. New vendored dependencies require separate approval and compatible licensing.

Pending: [Jolt Physics / ozz-animation proposal](PHYSICS-ANIMATION-PROPOSAL.md), pinned MIT sources for local physics/animation. No new library has been downloaded or added; explicit dependency approval is still required.

## F0 — Current limits and required guardrails

Tests below are acceptance criteria unless an actual run is recorded in the validation log.

### LIMIT-01 — Game-package authentication

Status: Partial: implemented and verified locally; native CI pending.

Source: [game_package.h](../../src/io/game_package.h), [sha256.h](../../src/io/sha256.h).

Plan: Agree key custody/trust first; then HMAC-SHA-256 over domain-separated header, manifest and payload, constant-time verification and explicit legacy unsigned policy. Never embed the secret in EXE/package/project.

Test: RFC 4231; header/payload/tag bit flips; wrong/missing keys; downgrade rejection; relocated valid game; no secret in archives/logs.

Implementation and actual checks: [package authentication validation](../validation/v1.4.0-package-auth.md), [local key usage](PACKAGE-AUTH.md).

### LIMIT-02 — Package 1 GiB, runtime 128 MiB, 4096-entry budgets

Status: Partial.

Source: [game_package.h](../../src/io/game_package.h), [v131_regressions.h](../../tests/v131_regressions.h).

Plan: Retain budgets, centralize checked arithmetic and add fast boundary cases plus a separate streaming stress job. Do not silently raise limits.

Test: N−1/N/N+1, UINT64_MAX, accumulated sizes, large manifests, empty/duplicate/overlapping ranges; transactional failure; retain sparse >3 GiB case.

### LIMIT-03 — Unsigned Windows; ad-hoc macOS, no notarization

Status: External requirement.

Source: [tests.yml](../../.github/workflows/tests.yml), [release.yml](../../.github/workflows/release.yml), [package_macos.py](../../tools/package_macos.py), [merge_macos.py](../../tools/merge_macos.py).

Plan: Prepare opt-in CI signing after native tests, before final hashes/attestation. Credentials only through GitHub Secrets; absent certificates retain truthful unsigned/ad-hoc status. Apple notarization is CI-only, never an application service.

Test: Absent/partial secret branches; Get-AuthenticodeSignature/signtool and codesign/spctl/stapler with real credentials; signed footer compatibility.

Preparation: [optional signing CI](RELEASE-SIGNING.md); policy tests passed locally. Real certificate-backed signing/notarization is not verified.

### LIMIT-04 — C# runs with user OS permissions

Status: Explicit retained contract.

Source: [csharp_bridge.h](../../src/scripting/csharp_bridge.h), [project_workspace.h](../../src/editor/project_workspace.h), [Program.cs](../../extensions/csharp/Program.cs).

Plan: Preserve session trust and deadlines; do not describe the worker as an OS sandbox. New engine APIs remain offline; arbitrary trusted C# retains OS access, including possible network access.

Test: No execution before trust; project/load trust lifecycle; timeout/crash/kill/reap/restart; never expose package secrets to scripts.

### LIMIT-05 — Dependency inventory is not a full advisory audit

Status: Partial.

Source: [dependency-runtime.json](../../docs/validation/dependency-runtime.json), [NOTICE](../../NOTICE), [packages.lock.json](../../extensions/csharp/packages.lock.json), [generate_sbom.py](../../tools/generate_sbom.py).

Plan: Preserve pinned/locked dependencies and GPL/NOTICE. The 2026-11-10 .NET snapshot is not a fresh advisory audit. Obtain approval for new vendored dependencies; do not download new packages in this stage.

Test: SBOM/hashes/licenses/lockfile; cached offline build; handle any concrete validated advisory separately.

### LIMIT-06 — SAX budgets are estimates, not a hard allocator

Status: Partial.

Source: [json_budget.h](../../src/io/json_budget.h), [scene_assets.h](../../src/io/scene_assets.h).

Plan: Keep preflight before DOM; budget new skins/channels/maps and memory amplification before allocation.

Test: Depth 95/96/97, node counts, many tiny fields, decoded-image 128 MiB ±1, ASan/UBSan and stress RSS bounds.

### LIMIT-07 — macOS worker memory limit is best-effort

Status: Partial.

Source: [worker_process.h](../../src/scripting/worker_process.h), [model_worker.h](../../src/scripting/model_worker.h), [export_worker.h](../../src/scripting/export_worker.h).

Plan: Retain CPU/deadline/transfer limits and use the same boundary for new heavy work; do not claim hard macOS RLIMIT_DATA enforcement.

Test: Nonterminating worker, 32 MiB exchange, oversized reply, pipe close/crash; native Windows Job/Linux limit and Mac evidence.

### LIMIT-08 — Fuzz/soak does not cover every format or state

Status: Partial.

Source: [soak_project_parser.py](../../tools/soak_project_parser.py), [endurance.yml](../../.github/workflows/endurance.yml).

Plan: Retain the 1000-cycle gate; add package/GLB/script/new-schema campaigns. One-hour parser CI 43703840148 succeeded; this is neither coverage-guided fuzzing nor a GPU test.

Test: Fixed seeds/corpus, minimized regressions, RSS/FD, deadlines, transactional loads; report duration and actual scope.

### LIMIT-09 — GPU/audio resource and driver coverage

Status: Limited evidence.

Source: [viewport_postprocess_tests.cpp](../../tests/viewport_postprocess_tests.cpp), [render_parity_tests.cpp](../../tests/render_parity_tests.cpp), [v131_regressions.h](../../tests/v131_regressions.h), [PERFORMANCE.md](../../docs/PERFORMANCE.md).

Plan: Preserve 1.3.1 lifecycle regressions; extend project/asset churn, live-resource counters and hardware coverage. Historical isolated benchmarks are not full-scene FPS.

Test: 100 resizes, 50 HDR/post switches, failed upload cleanup, 10 WAV + orphan; sustained named-GPU/driver runs with measurements; no zero-leak claim.

### LIMIT-10 — Schema 4 with schemas 1–3 migration and backups

Status: Partial; schema 4 migration implemented, native CI pending.

Source: [editor_project.h](../../src/editor/editor_project.h), [project_workspace.h](../../src/editor/project_workspace.h).

Plan: Approved schema 4 foundation; before the first manifest/tracked-script write, preserve exact old manifest and recoverable external script source. Keep prior backups, reject conflicts, recover read-only and extend new fields through validated round trips.

Test: Schema 1/2/3 goldens, idempotent migration, byte-identical backup, failed migration leaves source unchanged, explicit backward-reader contract.

Implementation: [project migration validation](../validation/v1.4.0-project-migration.md).

### LIMIT-11 — Document, script, undo and resource bounds

Status: Explicit retained contract.

Source: [script_document.h](../../src/editor/script_document.h), [editor_history.h](../../src/editor/editor_history.h), [editor_project.h](../../src/editor/editor_project.h), [scene_assets.h](../../src/io/scene_assets.h), [material_render.h](../../src/render/material_render.h), [audio_backend.c](../../extensions/audio/audio_backend.c).

Plan: Retain 32 MiB projects, 32 × 65535-byte scripts, 16 MiB encoded assets, 128 MiB/64 scene history, 128 MiB/256 textures, 64 voices/64 MiB encoded audio; account new data in shared budgets.

Test: UTF-8/CRLF boundaries, all slots, reject aggregate overflow before copying, undo warning and recovery after rejection; no silent truncation.

### LIMIT-12 — Trigger and simulation-time budgets are explicit

Status: Explicit retained contract.

Source: [gameplay_runtime.h](../../src/runtime/gameplay_runtime.h), [scene_tools.h](../../src/core/scene_tools.h).

Plan: Preserve 32768 actor×zone pairs, pause/reject without event loss, swept enter+exit in one tick, and two-second backlog with visible DroppedTime.

Test: 10000 overlaps, 32768 boundary, runtime overflow, enter/stay/exit/respawn ordering; heavy frame and deterministic replay.

### LIMIT-15 — Native platform, language-standard and MSVC scope

Status: Partial.

Source: [Makefile](../../Makefile), [Makefile.win](../../Makefile.win), [Makefile.mac](../../Makefile.mac), [tests.yml](../../.github/workflows/tests.yml).

Plan: Production C++17 and C17 build rules are set; retain ABI and compatibility fixtures. Verify Linux GCC, Windows MinGW and Apple Clang/macOS 15+ natively; no MSVC claim without a supported build.

Test: Native CI on each platform, C/C++ ABI/header checks, strict warnings and C17 compile; expand claims only with evidence.

### LIMIT-16 — Stale help and historical audit statements

Status: Documentation fixed in Stage 0.

Source: [topics.json](../../docs/manual/topics.json), [AUDIT-REMEDIATION.md](../../docs/AUDIT-REMEDIATION.md), [1.3.0-plan.md](../../docs/work/1.3.0-plan.md).

Plan: Correct Lua file/process-library claims and obsolete 8192/4096 trigger limits; mark historical remediation as superseded. Regenerate EN/RU and embedded help together.

Test: test_release_safety.py, build_manual.py, idempotent regeneration; compare claims with lua_sandbox/gameplay_runtime.

### GUARD-01 — Preserve Lua, MCP and anchored-I/O boundaries

Status: Required guardrail.

Source: [lua_sandbox.h](../../src/scripting/lua_sandbox.h), [studio_mcp.h](../../src/scripting/studio_mcp.h), [secure_asset_io.h](../../src/io/secure_asset_io.h).

Plan: Keep Lua 256 MiB/instruction/project-only-require limits; local stdio MCP read-only with separate write/lua grants; no-follow/handle-relative/atomic rename. No new services/accounts/telemetry.

Test: test_audit_security.py, test_mcp_capabilities.py, v13 reparse/symlink regressions; route new APIs through the same boundary.

### GUARD-02 — 1.3.1 regressions and release gate

Status: Required guardrail.

Source: [v131_regressions.h](../../tests/v131_regressions.h), [core_tests.cpp](../../tests/core_tests.cpp), [viewport_postprocess_tests.cpp](../../tests/viewport_postprocess_tests.cpp), [render_parity_tests.cpp](../../tests/render_parity_tests.cpp), [test_release_safety.py](../../tools/test_release_safety.py), [release.yml](../../.github/workflows/release.yml).

Plan: Keep all nine 1.3.1 regression groups. Publish only exact-SHA native CI + sanitizers + graphics + packaged C# gameplay + fresh UI video; four assets, internal SBOM/checksums, immutable tags.

Test: Full gate; reject wrong SHA/failed job/stale video/extra asset; absent credentials must not yield verified-signing claims.


## P1 — Core update and related limits

Tests below are acceptance criteria unless an actual run is recorded in the validation log.

### LIMIT-13 — Legacy indices, duplicate names and Join restrictions

Status: Explicit retained contract.

Source: [scene_tools.h](../../src/core/scene_tools.h), [luaryx_runtime.h](../../src/scripting/luaryx_runtime.h), [Scene.cs](../../extensions/csharp/Scene.cs).

Plan: Use stable IDs in new APIs; preserve legacy first-match find_object and ambiguity errors in new find. Join must keep rejecting data it cannot preserve.

Test: Object removal/reordering, duplicate names, stable-ID round trip; failed Join preserves materials/animation/properties.

### P1-ANIM-01 — Skeleton and skinning

Status: Not implemented.

Source: [scene_core.h](../../src/core/scene_core.h), [studio_mesh_types.h](../../src/core/studio_mesh_types.h), [material_render.h](../../src/render/material_render.h).

Plan: Add rigs/joints/inverse-bind/weights and bounded skinning; CPU reference before GPU path.

Test: Bind pose, 1–4 weights, indices/NaN, transformed parents, CPU/GPU parity, save/undo/game.

### P1-ANIM-02 — Inverse kinematics

Status: Not implemented.

Source: [scene_animation.h](../../src/core/scene_animation.h).

Plan: Deterministic bounded two-bone IK, then constrained chains.

Test: Reachable/unreachable goals, zero-length bones, joint limits, no NaN, iteration cap.

### P1-ANIM-03 — Blending and state machines

Status: Not implemented.

Source: [scene_animation.h](../../src/core/scene_animation.h).

Plan: Add clips/parameters/transitions/blend masks with explicit transition/interruption semantics.

Test: Transition boundaries, blend endpoints, quaternion shortest path, looping/replay.

### P1-ANIM-04 — Easing curves

Status: Partial.

Source: [animation_core.h](../../src/core/animation_core.h), [animation_tests.c](../../tests/animation_tests.c).

Plan: Ten bounded C17 curves are implemented in timeline/Lua/schema 4/history/packed runtime; preserve legacy IDs 0–2. Complete native CI and sanitizer validation.

Test: 0/1 endpoints, expected monotonicity, NaN/zero span, legacy goldens.

[Actual easing validation](../validation/v1.4.0-easing.md).

### P1-ANIM-05 — Shape keys

Status: Not implemented.

Source: [studio_mesh_types.h](../../src/core/studio_mesh_types.h), [scene_animation.h](../../src/core/scene_animation.h).

Plan: Bounded mesh deltas/weights, normals and composition with skinning.

Test: Topology/size validation, weight blends, negative/excessive values, round trip/GL parity.

### P1-ANIM-06 — Animation events and root motion

Status: Not implemented.

Source: [scene_animation.h](../../src/core/scene_animation.h), [game_runtime.h](../../src/runtime/game_runtime.h).

Plan: Timeline-crossing events with loop/wrap semantics and root deltas through physics/controller.

Test: Exactly-once events, large dt, reverse/loop, Stop/restart, colliding root motion.

### P1-PHYS-01 — Dynamic rigid bodies and impulses

Status: Partial.

Source: [scene_tools.h](../../src/core/scene_tools.h), [scene_core.h](../../src/core/scene_core.h).

Plan: Dynamic/Mass/Velocity and AABB response exist; add rotation/inertia/angular velocity/contact impulses with explicit legacy-backend compatibility.

Test: Conservation, stacking/resting/friction, fixed-step replay, legacy 3840-step/2000-bound fixtures.

### P1-PHYS-02 — Constraints

Status: Not implemented.

Source: [scene_tools.h](../../src/core/scene_tools.h).

Plan: Stable-ID distance/hinge constraints, bounded solver iterations and body-removal semantics.

Test: Drift/limits, inactive/deleted bodies, closed loops, serialization/undo.

### P1-PHYS-03 — Sphere/capsule/convex colliders

Status: Not implemented.

Source: [scene_tools.h](../../src/core/scene_tools.h), [spatial_index.h](../../src/core/spatial_index.h).

Plan: Validated collider shapes and narrow phase; AABBs remain broad-phase bounds.

Test: Analytic contacts, rotated/degenerate convex, negative scales, swept/overlap consistency.

### P1-PHYS-04 — Raycast/overlap APIs and collision layers/masks

Status: Partial.

Source: [game_bindings.h](../../src/scripting/game_bindings.h), [gameplay_runtime.h](../../src/runtime/gameplay_runtime.h).

Plan: trigger_contains exists; add shared stable-ID/normal/distance queries and Lua/C# filtering.

Test: Closest/all hits, inside/zero rays, normalization, mask symmetry, deleted entities, API parity.

### P1-PHYS-05 — Character slopes and steps

Status: Partial.

Source: [scene_tools.h](../../src/core/scene_tools.h), [gameplay_runtime.h](../../src/runtime/gameplay_runtime.h).

Plan: WASD/jump/grounded AABB exists; add capsule sweeps, slope limits, step-up/down/contact normals.

Test: Slopes around limit, step ±epsilon, ceilings, moving platforms, root motion/tunnelling.

### P1-PHYS-06 — BVH broad phase

Status: Partial.

Source: [spatial_index.h](../../src/core/spatial_index.h).

Plan: Current spatial grid has large-object fallback; add measured BVH/refit while keeping grid as reference/fallback.

Test: Query differential vs brute force, dense/large worlds, refit/removal, measured perf/memory.

### P1-RENDER-01 — PBR texture maps

Status: Partial.

Source: [studio_mesh_types.h](../../src/core/studio_mesh_types.h), [scene_shadows.h](../../src/render/scene_shadows.h), [material_render.h](../../src/render/material_render.h), [scene_assets.h](../../src/io/scene_assets.h).

Plan: Base texture and scalar metallic/roughness exist; add normal/roughness/metallic/AO/emissive, tangents and shared import/cache budgets.

Test: glTF channels, sRGB/linear, missing/invalid maps, tangent orientation, GL fallback, round-trip/undo.

### P1-RENDER-02 — Point/spot lights and shadows

Status: Partial.

Source: [scene_shadows.h](../../src/render/scene_shadows.h).

Plan: Sun PCF shadows exist; add bounded point/spot lights, culling and shadow atlas/cubemaps with capability fallback.

Test: Occlusion, spot cones/bias, light caps, unsupported capabilities and resource cleanup.

### P1-RENDER-03 — Sorted transparency

Status: Not implemented.

Source: [material_render.h](../../src/render/material_render.h), [render_geometry.h](../../src/render/render_geometry.h).

Plan: Separate opaque/masked/blended queues with correct depth writes and stable back-to-front sorting.

Test: Overlapping alpha meshes, equal depth, camera rotation, cached/legacy image parity.

### P1-RENDER-04 — Skybox and fog

Status: Partial.

Source: [render_showcase.h](../../src/render/render_showcase.h), [editor_ui.h](../../src/editor/editor_ui.h).

Plan: Gradient sky presets exist; add local cubemap assets and fog in the linear HDR pipeline.

Test: Cube faces/orientation, missing assets, fog distances, HDR/Office fallback and relocated package.

### P1-RENDER-05 — Instancing and batching

Status: Partial.

Source: [render_geometry.h](../../src/render/render_geometry.h), [material_render.h](../../src/render/material_render.h).

Plan: VBO/material batches exist; add instance groups and extension-gated instancing with GL2.1 fallback.

Test: Image parity, changing transforms/materials, capability on/off, draw calls/cache churn.

### P1-RENDER-06 — Level of detail

Status: Not implemented.

Source: [render_geometry.h](../../src/render/render_geometry.h), [studio_mesh_types.h](../../src/core/studio_mesh_types.h).

Plan: Explicit mesh LOD levels, distance/error policy and hysteresis; generation stays local.

Test: Threshold ±epsilon, no flicker, bounds/culling, invalid levels, memory/standalone parity.

### P1-MODEL-01 — Extrude/inset/bevel

Status: Partial.

Source: [mesh_modeling.h](../../src/core/mesh_modeling.h), [editor_modeling.h](../../src/editor/editor_modeling.h).

Plan: Extrude/bevel exist; bevel requires a closed consistently oriented mesh. Add inset, region/individual semantics and attribute preservation.

Test: Manifold/boundary/concave faces, UV/normals/colors, invalid selections, budgets/atomic undo.

### P1-MODEL-02 — Loop cut and knife

Status: Not implemented.

Source: [mesh_modeling.h](../../src/core/mesh_modeling.h), [editor_modeling.h](../../src/editor/editor_modeling.h).

Plan: Topology operations with preview/commit, validation and UV/color/seam propagation.

Test: Closed/open/nonmanifold topology, vertex/edge hits, canceled preview, triangulation/undo.

### P1-MODEL-03 — Subdivision, mirror and array

Status: Partial.

Source: [mesh_modeling.h](../../src/core/mesh_modeling.h), [studio_mesh_types.h](../../src/core/studio_mesh_types.h).

Plan: Already present; retain levels 1–4/array 2–64 caps, verify boundary/UV/seam contracts and fix concrete issues instead of reimplementation.

Test: Existing modeling tests plus boundary goldens, mirrored winding, array normals, apply/undo.

### P1-MODEL-04 — Snapping and proportional editing

Status: Partial.

Source: [editor_transform.h](../../src/editor/editor_transform.h), [editor_gizmo.h](../../src/editor/editor_gizmo.h), [editor_modeling.h](../../src/editor/editor_modeling.h).

Plan: Grid snapping exists; add vertex/edge snapping with visible targets and bounded proportional falloff.

Test: Local/world, hidden geometry, large coordinates, precision, radius/falloff endpoints and undo.

### P1-MODEL-05 — Vertex-color editing

Status: Partial.

Source: [studio_mesh_types.h](../../src/core/studio_mesh_types.h), [mesh_modeling.h](../../src/core/mesh_modeling.h), [editor_modeling.h](../../src/editor/editor_modeling.h).

Plan: Vertex-color storage/render/GLB support exists; add explicit paint/fill/edit controls and propagation through new operations.

Test: RGB/color-space contract, selected vertices, topology interpolation, round-trip and undo.

### P1-SAVE-01 — Versioned game-save slots

Status: Not implemented.

Source: [game_project.h](../../src/runtime/game_project.h), [secure_asset_io.h](../../src/io/secure_asset_io.h).

Plan: Dedicated per-game local save root and bounded versioned slots; atomic writes, migrations and recovery; editor recovery is not game saves.

Test: Traversal/NUL/symlink/reparse, interrupted writes, old/new versions, malformed state, relocated game; Lua/C# parity.

### P1-INPUT-01 — Input rebinding

Status: Partial.

Source: [game_bindings.h](../../src/scripting/game_bindings.h), [editor_preferences.h](../../src/editor/editor_preferences.h).

Plan: input_key/scancodes exist; add action maps, press/release/held, per-user overrides and editor conflict handling.

Test: Duplicate/unbound bindings, modifiers, focus/text capture, UTF-8 names, local persistence/legacy compatibility.

### P1-INPUT-02 — SDL GameController input

Status: Not implemented.

Source: [main.cpp](../../src/main.cpp), [game_bindings.h](../../src/scripting/game_bindings.h).

Plan: Use existing SDL2 for hotplug, axes/deadzones, player assignment and offline mappings.

Test: Virtual-controller fixtures, hotplug/unplug, axis extremes, focus, multiple devices/keyboard coexistence.

### P1-INPUT-03 — Local split screen

Status: Not implemented.

Source: [main.cpp](../../src/main.cpp), [game_gui.h](../../src/runtime/game_gui.h), [viewport_target.h](../../src/render/viewport_target.h).

Plan: Multiple local players/cameras/viewports/input contexts, independent GUI and shared simulation without networking.

Test: 2/4 views, resize/HDR, GUI clipping, input isolation, camera aspects and cleanup.


## P2 — Offline features and stabilization

Tests below are acceptance criteria unless an actual run is recorded in the validation log.

### LIMIT-14 — Long-path support does not remove field caps

Status: Partial.

Source: [asset_io.h](../../src/io/asset_io.h), [editor_project.h](../../src/editor/editor_project.h), [script_document.h](../../src/editor/script_document.h), [windows.manifest](../../assets/windows.manifest).

Plan: Preserve wide/extended paths and anchored I/O; widen UI/format fields deliberately instead of disabling length checks.

Test: Native Unicode >260 paths, 511/512/8192 field bounds, case aliases/reparse races; reject without truncation.

### P2-AUDIO-01 — 3D audio, attenuation and mixer

Status: Partial.

Source: [audio_backend.c](../../extensions/audio/audio_backend.c), [studio_sounds.h](../../src/runtime/studio_sounds.h), [editor_audio.h](../../src/editor/editor_audio.h).

Plan: Spatial position/listener/loop already exist; expose attenuation/cones and bounded buses/groups with gain/mute/ducking.

Test: Null-device PCM, distance/pan/cones, bus cycles/clipping, Stop/orphan cleanup and saved settings.

### P2-AUDIO-02 — Disk-streamed music and loop points

Status: Partial.

Source: [audio_backend.c](../../extensions/audio/audio_backend.c), [secure_asset_io.h](../../src/io/secure_asset_io.h).

Plan: Current decoder streams from retained encoded bytes; add bounded ring/file streaming from pinned handles and loop points, not unsafe path reopens.

Test: Long files with bounded RAM, underrun/seek/loop/end, corrupt input/file swaps and shutdown.

### P2-GUI-01 — Anchors, layout and widgets

Status: Partial.

Source: [game_gui.h](../../src/runtime/game_gui.h), [game_state.h](../../src/runtime/game_state.h), [editor_gui.h](../../src/editor/editor_gui.h).

Plan: 128 flat label/button/panel/progress widgets exist; add tree layout, anchors, slider/dropdown/scroll views and bounded input handling.

Test: DPI/aspect resize, nesting/cycle caps, clipping/hit order, keyboard/controller focus, serialization/undo.

### P2-GUI-02 — Themes, localization and controller navigation

Status: Not implemented.

Source: [game_gui.h](../../src/runtime/game_gui.h), [editor_gui.h](../../src/editor/editor_gui.h).

Plan: Local string-ID/fallback tables, theme resources and focus navigation; EN/RU docs are not game localization.

Test: Missing keys, UTF-8/font coverage, disabled/hidden focus, no remote font/service lookup.

### P2-SCRIPT-01 — Lua/C# hot reload

Status: Partial.

Source: [code_editor.h](../../src/editor/code_editor.h), [csharp_bridge.h](../../src/scripting/csharp_bridge.h), [luaryx_runtime.h](../../src/scripting/luaryx_runtime.h).

Plan: Explicit Run/reinit exists; add validated compile-then-swap, state-transfer contract and rollback while preserving trust/deadline/confinement.

Test: Syntax/type errors keep working runtime; on_stop/on_start exactly once, module cycles, changed-code trust.

### P2-SCRIPT-02 — Luaryx input/timer/tween/raycast/save APIs

Status: Partial.

Source: [luaryx_runtime.h](../../src/scripting/luaryx_runtime.h), [game_bindings.h](../../src/scripting/game_bindings.h), [Scene.cs](../../extensions/csharp/Scene.cs).

Plan: Expose new input/physics/save services through stable IDs; timers/tweens use simulation time and clear on Stop/load.

Test: Pause/step/restart, canceled callbacks, caps/recursion, destroyed entities, Lua/C# parity/sandbox enforcement.

### P2-SCRIPT-03 — Completion, templates and script profiler

Status: Partial.

Source: [code_editor.h](../../src/editor/code_editor.h), [script_document.h](../../src/editor/script_document.h), [editor_debug.h](../../src/editor/editor_debug.h).

Plan: Basic templates/highlighting/diagnostics exist; add offline API completion and bounded per-script timing/allocation measurements without telemetry.

Test: Unicode cursor ranges, cancellation/no network, stale diagnostics, per-script budgets/profiler-off overhead.

### P2-ASSET-01 — Animated/skinned GLB/GLTF

Status: Partial.

Source: [scene_assets.h](../../src/io/scene_assets.h), [studio_mesh_types.h](../../src/core/studio_mesh_types.h).

Plan: Current import is static GLB with unsupported animation/skin notices. Add .gltf/external buffers/images confined to import root, joints/channels/weights and export contracts.

Test: glTF fixtures/validator, path/URI/data bounds, sparse accessors, hierarchy cycles, key interpolation, round-trip/budget failures.

### P2-ASSET-02 — Asset browser, hot reload and dependencies

Status: Partial.

Source: [project_workspace.h](../../src/editor/project_workspace.h), [scene_assets.h](../../src/io/scene_assets.h), [material_render.h](../../src/render/material_render.h).

Plan: GLB/audio drop and texture mtime keys exist; add a local asset index, preview cache, dependency graph and debounced invalidation.

Test: Rename/delete/change, dependency cycles, stale preview jobs, failed reload rollback, same-name files/confined handles.

### P2-UX-01 — Outliner, parenting, selection and gizmos

Status: Partial.

Source: [editor_ui.h](../../src/editor/editor_ui.h), [editor_selection.h](../../src/editor/editor_selection.h), [editor_gizmo.h](../../src/editor/editor_gizmo.h).

Plan: Search/multiselect/local-world gizmos already exist. Add stable-ID parenting, keep-world/reparent semantics and cycle prevention.

Test: Reparent mirrored/nonuniform transforms, recursive deletion/undo, filtered multiselect/local-world compatibility.

### P2-UX-02 — Orthographic views, view cube, bookmarks and measuring

Status: Partial.

Source: [editor_camera_math.h](../../src/editor/editor_camera_math.h), [editor_gizmo.h](../../src/editor/editor_gizmo.h), [editor_transform.h](../../src/editor/editor_transform.h).

Plan: Perspective/free camera exists; add orthographic projection/picking, cube navigation, local bookmarks and measuring.

Test: Ortho/perspective picking, aspect/zoom extremes, gizmo matrices, bookmark round-trip, units/large coordinates.

### P2-UX-03 — Profiler overlay, console and hotkey editor

Status: Partial.

Source: [editor_debug.h](../../src/editor/editor_debug.h), [editor_preferences.h](../../src/editor/editor_preferences.h), [editor_ui.h](../../src/editor/editor_ui.h).

Plan: FPS/render counters/console exist; add bounded memory/timing overlay and shortcut-conflict editor with local preferences.

Test: Counter reconciliation, profiler-off overhead, bounded logs, focus-aware shortcuts/defaults, no telemetry.

### P2-UX-04 — Project templates and tutorial scenes

Status: Partial.

Source: [example_game.h](../../src/examples/example_game.h), [project_workspace.h](../../src/editor/project_workspace.h).

Plan: Gem Trail sample exists; add shipped offline templates/tutorial scenes with current EN/RU instructions.

Test: Every template opens/saves/compiles/packages successfully, no missing/network assets.

### P2-NODES-01 — Geometry-node stabilization

Status: Partial.

Source: [geometry_nodes.h](../../src/core/geometry_nodes.h), [editor_nodes.h](../../src/editor/editor_nodes.h).

Plan: Eight types, one geometry input/node, 32-node cap and preview/commit exist. Add typed sockets/nodes/undo and usable editing without silent data loss.

Test: Cycles, disconnected/invalid types, graph budgets, preview cancellation/stale workers, save/undo/bake parity.

### P2-CACHE-01 — Cached rendering and quality presets

Status: Partial.

Source: [render_geometry.h](../../src/render/render_geometry.h), [material_render.h](../../src/render/material_render.h), [editor_preferences.h](../../src/editor/editor_preferences.h).

Plan: Caches/presets and oversubscription fallback exist; extend invalidation to maps/skins/instances and expose capability/results.

Test: Cached/reference image parity, undo/skin/material edits, active-set overflow, driver fallbacks/resource counters.


## P3 — Only after P1/P2

Tests below are acceptance criteria unless an actual run is recorded in the validation log.

### P3-STRETCH-01 — Terrain / particles / decals / A* / 2D

Status: After P1/P2.

Source: [scene_core.h](../../src/core/scene_core.h).

Plan: Do not start before agreed P1/P2 completion; later use separate offline slices with budgets and format/API contracts.

Test: Separate acceptance corpus/native gates for each subsystem; not implemented or tested in this stage.

## All 40 original audit entries

Original statuses belong to released 1.3.0/1.3.1 and are not future-update results. All entries are retained in JSON; current limits are listed above. `fixed` does not imply the absence of other defects.

| ID | Source status | Evidence |
| --- | --- | --- |
| SEC-01 | fixed | tests/v13_regressions.h; tools/test_audit_security.py |
| SEC-02 | fixed | tools/soak_project_parser.py; src/io/json_budget.h |
| SEC-03 | fixed | tests/worker_process_tests.cpp; tools/test_audit_security.py |
| GAME-01 | fixed | tests/v13_regressions.h |
| GAME-02 | fixed | tests/v13_regressions.h |
| REL-01 | fixed | src/editor/editor_history.h; src/editor/code_editor.h |
| REL-02 | fixed | src/editor/editor_history.h; src/editor/editor_ui.h |
| REL-03 | fixed | src/main.cpp; src/editor/editor_preferences.h |
| MEM-01 | fixed | src/render/material_render.h; native rendering tests |
| MEM-02 | fixed | src/io/game_package.h; tools/test_game_package.py |
| MEM-03 | fixed | src/editor/editor_history.h |
| PERF-01 | fixed | tests/physics_differential.cpp; src/core/spatial_index.h |
| PERF-02 | fixed | tests/v13_regressions.h |
| SEC-04 | external_requirement | Release signing verification |
| SEC-05 | partial | SBOM.cdx.json; extensions/csharp/packages.lock.json |
| SEC-06 | partial | tools/soak_project_parser.py; .github/workflows/endurance.yml |
| SEC-07 | contract | src/io/game_package.h; release provenance |
| SEC-08 | fixed | tools/test_mcp_capabilities.py |
| GAME-03 | contract | tests/game_features_tests.h |
| REL-04 | fixed | src/io/game_package.h |
| REL-05 | fixed | src/editor/editor_project.h |
| REL-06 | fixed | tests/v13_regressions.h; src/scripting/luaryx_runtime.h |
| REL-07 | fixed | src/scripting/luaryx_runtime.h; extensions/csharp/Scene.cs |
| REL-08 | fixed | tests/engine_tests.cpp; src/core/scene_tools.h |
| REL-09 | fixed | tests/v13_regressions.h |
| MEM-04 | fixed | src/render/material_render.h |
| MEM-07 | fixed | src/render/render_geometry.h |
| MEM-05 | fixed | extensions/audio/audio_backend.c; native audio tests |
| MEM-06 | limited_evidence | parser-soak.json; sanitizer CI |
| PERF-05 | fixed | src/io/scene_assets.h |
| PERF-04 | fixed | src/runtime/gameplay_runtime.h |
| PERF-03 | fixed | src/core/scene_tools.h; src/editor/editor_debug.h |
| PERF-06 | fixed | src/scripting/export_worker.h |
| COMP-01 | fixed | assets/windows.manifest; src/io/asset_io.h |
| COMP-02 | contract | src/io/scene_assets.h; docs/en/README.md |
| COMP-03 | contract | src/io/game_package.h; tests/engine_tests.cpp |
| QA-01 | limited_evidence | CI and Verification.mp4 |
| QA-02 | limited_evidence | Native CI matrix |
| COMP-04 | fixed | LICENSE; NOTICE; CONTRIBUTING.md |
| QA-03 | fixed | tools/package_release.py; .github/workflows/release.yml |

## Required 1.3.1 regressions

| ID | Cases | Source |
| --- | --- | --- |
| R131-GPU | 100 resize; 50 HDR/post switches; scoped GL deletion | [viewport_postprocess_tests.cpp](../../tests/viewport_postprocess_tests.cpp) |
| R131-TEX | bad_alloc injection; deleted texture ID; zero accounting; successful retry | [render_parity_tests.cpp](../../tests/render_parity_tests.cpp) |
| R131-AUDIO | 10 WAV clips; preview Stop; repeated Play/Stop; orphan voice; zero live voices | [v131_regressions.h](../../tests/v131_regressions.h) |
| R131-BOUNDS | NaN/+Inf/-Inf in every coordinate; mirrored finite control | [core_tests.cpp](../../tests/core_tests.cpp) |
| R131-DIRTY | camera/widget/color -0 equals +0; real text edit changes hash | [v131_regressions.h](../../tests/v131_regressions.h) |
| R131-OFFSETS | valid sparse >3 GiB; UINT64_MAX footer rejected transactionally; 128 MiB runtime cap | [v131_regressions.h](../../tests/v131_regressions.h) |
| R131-LUA | embedded NUL/control/oversize suffix rejected; UTF-8/spaces/default/empty names retained | [v131_regressions.h](../../tests/v131_regressions.h) |
| R131-GUIDE | GUIDE terminator and unclosed fences rejected before output; valid lists/Unicode | [test_release_safety.py](../../tools/test_release_safety.py) |
| R131-MAC | external paths and earlier external rpath candidate rejected; worker-relative valid control | [test_release_safety.py](../../tools/test_release_safety.py) |

## Evidence and release

Native 1.3.1 baseline CI: https://github.com/kittkanev/Lunoryx/actions/runs/43246269279. Parser endurance: https://github.com/kittkanev/Lunoryx/actions/runs/43703840148. This is historical evidence for the exact baseline SHA; Windows/macOS have not been rerun inside this Linux workspace.

Release gate: native CI per claimed platform, ASan/UBSan/leaks, graphics lifecycle, relocated packaged C# gameplay and fresh actual UI video. Exactly four uploaded assets: Windows ZIP, universal macOS ZIP, Verification.mp4, Audit-report.html; SBOM/checksums/licenses inside archives. No certificates supplied: no Authenticode/Developer ID/notarization claims. Report new security defects privately to the owner; do not publish issues/exploits.
