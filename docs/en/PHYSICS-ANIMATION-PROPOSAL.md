# P1 dependency proposal — approval required

No library below has been downloaded, vendored or added to the build. The owner requires explicit approval for new dependencies.

| Component | Proposed source pin | License | Use |
| --- | --- | --- | --- |
| Jolt Physics 5.7.0 | `jrouwe/JoltPhysics`, `cf16769bb0ede9c45d57d06068ff0689e9702f5c` | MIT | Rigid bodies, contacts/constraints, shapes, broad phase, ray/overlap queries and character support |
| ozz-animation 0.17.0 | `guillaumeblanc/ozz-animation`, `744eb9d99f606eda849acb0b1204f7a3dc20bca1` | MIT | Skeleton pose sampling/blending, skinning reference and IK building blocks |

The pins and upstream license files were inspected through read-only GitHub metadata. Jolt's `Jolt/Jolt.cmake` explicitly requires C++17. Native Lunoryx integration, performance, safety and platform compatibility are **not yet verified**.

If approved:

1. Vendor pinned source and unmodified licenses locally; update NOTICE/SBOM. Do not introduce a runtime downloader, cloud API, account, telemetry or service.
2. Build static libraries from local source on existing GCC/MinGW/Apple Clang CI. Disable samples, viewers, tools, external FBX SDK, tests that fetch third-party frameworks and configure-time downloads. Existing tinygltf remains the import boundary.
3. Explicitly disable optional AVX/AVX2/AVX-512/FMA/F16C and host-native compiler tuning for portable x64 builds. Use the appropriate ARM path on Apple Silicon. Do not imply cross-platform bitwise determinism without actual tests.
4. Keep an explicit legacy AABB path for old projects and their differential tests. New physics needs a tested schema migration, stable IDs, budgets and editor/script APIs, not just a linked library.
5. Build the skeleton/editor/glTF/state-machine/root-motion/event/shape-key integrations as separate tested vertical slices. ozz is not a complete animation editor and does not implement all P1 features automatically.
6. Require native CI, sanitizers, lifecycle tests, script interoperability, persistence/undo/migration fixtures and packaged gameplay before release claims.

Alternatives: approve only Jolt and implement animation locally; or add neither and implement both kernels locally. The latter avoids dependencies but requires a substantially larger new solver/animation implementation and validation effort. Existing Manifold/xatlas do not supply the requested rigid-body solver or skeleton runtime.

References: [Jolt release](https://github.com/jrouwe/JoltPhysics/releases/tag/v5.7.0), [Jolt license](https://github.com/jrouwe/JoltPhysics/blob/cf16769bb0ede9c45d57d06068ff0689e9702f5c/LICENSE), [ozz release](https://github.com/guillaumeblanc/ozz-animation/releases/tag/0.17.0), [ozz license](https://github.com/guillaumeblanc/ozz-animation/blob/744eb9d99f606eda849acb0b1204f7a3dc20bca1/LICENSE.md).
