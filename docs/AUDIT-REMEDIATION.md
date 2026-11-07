# Audit remediation

Source: user-supplied `Lunoryx-audit.md`, reviewing 1.2.1. The original audit was
static analysis, not an execution report. This document distinguishes implemented
repairs, design limitations and claims for which the audit supplies no proof.

## Implemented changes

| Audit items | Change |
| --- | --- |
| SEC-01, SEC-03 | All user entry points use a restricted Lua state and text-only chunks. No `io`, `package`, `debug`, process execution, filesystem loaders or dynamic `load`. Safe time functions remain. |
| SEC-02 | A 256 MiB allocator budget is shared by all coroutines. Instruction exhaustion remains sticky across protected calls. Coroutine resume/wrap reinstall the shared hook. Script `__gc` finalizers are rejected because Lua disables hooks while running them. String repetition and native pattern matching have explicit resource bounds. Native object creation/model commands respect object and mesh budgets. |
| SEC-04 | Project texture, sound and icon references stay inside the project directory. Lua/MCP filesystem operations stay inside the process working directory. Canonical checks cover existing symlinks; network/device paths are rejected before normalization. Both GLB existence probes and reads are confined to the import directory. Saving explicitly chosen external assets embeds them so the editor's own files remain loadable. |
| SEC-05 | Windows Lua moves from 5.4.6 to 5.4.8 and SDL2 from 2.30.0 to 2.32.10. Download hashes are pinned. See the remaining dependency qualifications below. |
| SEC-06 | Test environment controls require `LUNORYX_TESTING`. Production and test executables/audio objects have separate build targets. Packaged player mode cannot enter MCP. Published sample games are built from the production executable. |
| SEC-07, BUG-09 | Package detection reads the footer and payload, not the whole executable. PE certificate-table offsets and alignment padding are recognized so an overlay followed by Authenticode data remains a game. CRC32 still detects accidental corruption; it is not an authenticity claim. |
| SEC-08 | ELF builds request stack protection, fortified libc, PIE, RELRO and immediate binding. PE builds explicitly request DEP, ASLR and high-entropy ASLR; verification checks their header flags. |
| SEC-09 | Actions use immutable commit SHAs. Published release tags/assets cannot be force-replaced. Release write privileges are job-scoped and release artifacts receive build-provenance attestations. Download scripts require HTTPS/TLS 1.2. Artifact naming uses Lunoryx. |
| SEC-10 | Project DOM construction has a node/depth budget; binary data no longer expands into a JSON node per byte. Triangulation has a work budget. Package detection is bounded. Existing per-operation geometry budgets remain in force. |
| BUG-01 | New binary fields use strict, bounded base64 objects and compact JSON; legacy numeric byte arrays remain readable. |
| BUG-02, BUG-04 | Game packaging and GLB export deduplicate identical encoded textures, including imported embedded images. |
| BUG-03 | GLB is streamed into an exclusively created sibling temporary file and replaces its destination only after a successful close. Project writes use the same exclusive temporary-file mechanism. |
| BUG-05 | GLB skips empty derived meshes; ordinary primitives are still exported. |
| BUG-06, BUG-07 | Each MCP call returns its own console output. Invalid output bytes are replaced when encoding JSON instead of invalidating the response. |
| BUG-08 | Audio loading uses the Windows wide API; captures use the UTF-8 aware file wrapper. Temporary output cleanup also uses wide paths. |
| BUG-10 | Drag-and-drop frees its SDL allocation in both modes and only recognized audio extensions reach the decoder. |
| BUG-11 | An empty game-icon setting preserves the application icon. |
| BUG-12 | Script/MCP/project transforms share bounded validation. Negative, nonzero scales remain supported. Script object-growth paths use the same 10,000-object limit and a scene mesh budget. |
| BUG-13 | Unknown primitive names fail instead of silently creating a cube. |
| BUG-14 | F5 creates a fresh Lua state, matching Play's startup behavior. MCP intentionally preserves session globals. |
| BUG-15 | Console lines and Lua print assembly are bounded. |
| BUG-16 | Runtime-overwrite protection checks file identity, including hard links and Windows file IDs. |
| BUG-17 | Loading uses provisional IDs and reserves real IDs only after document validation. Rejected loads do not advance the global object-ID counter. |
| BUG-18, BUG-19 | Audio ID zero cannot resolve an empty voice. Asset existence checks return false on path conversion failures. |
| BUG-20 | Triangulation removes collinear points that lie between their neighbours, while keeping face/index ordering consistent. Invalid self-intersections still fail explicitly. |
| DEBT-04, DEBT-05, DEBT-06 | Clean removes the omitted test binaries. Added hostile MCP and integration regressions. `unwrap` is described in the MCP tool schema. |
| DEBT-07 | Removed a redundant Rust `unwrap` in the production weld path; arithmetic/input bounds remain checked. No concrete overflow was demonstrated by the audit. |
| DEBT-08 | Dirty editor documents get an atomic recovery snapshot every minute, outside Play. `Open recovery` opens the snapshot for the current project; snapshots can also be opened as regular `.lunoryx` files. Writing a snapshot does not mark the editor document saved. |

## Compatibility and limits

New base64 fields extend schema 2. This version reads old numeric arrays, but older
versions cannot read the new binary-field representation. Keep an original copy
when migrating a project. The existing 32 MiB document and 16 MiB embedded-asset
limits still apply. External resources selected through the editor are embedded
when saving outside their directory; a malicious legacy project cannot request
that trust automatically. Relocate or explicitly import its external resources.

Lua scripts have the engine API, base/coroutine/table/string/math/utf8 libraries,
and `os.time/clock/date/difftime`. They cannot install finalizers or load native
modules/files. Excessive string patterns fail conservatively before entering
Lua's native backtracking matcher. Filesystem access is an intentional breaking
change for scripts that relied on unrestricted Lua libraries. An arbitrary native
EXE remains executable native code; the Lua sandbox cannot make third-party EXEs
safe to run. Individual native geometry operations are bounded by geometry/work
limits, not a hard wall-clock timeout.

Recovery snapshots use `<project>.autosave.lunoryx`, never overwrite the main
project, and may contain the last minute of edits rather than the latest keystroke.
For an unnamed document the default is `project.lunoryx.autosave.lunoryx` in the
working directory. Open a snapshot and use Save As to restore the main document.

## Claims and work that remain separate

- **SEC-05:** Version age alone does not establish a vulnerability. The supplied
  report gives no reproducible CVE for the vendored stb_vorbis/tinygltf copies.
  Upstream stb_vorbis is also labelled 1.22. A full dependency-advisory audit and
  migration from the ImGui development snapshot are not claimed complete here.
- **SEC-07 / DEBT-09:** No signing certificate was supplied. Executables are not
  newly Authenticode-signed; certificate-table parsing is tested independently.
  Provenance attestations require the GitHub workflow to run after publication.
- **SEC-10 / DEBT-05:** New focused malformed-input regressions are not a sustained
  fuzzing campaign. Native operations do not promise preemption or a wall-clock
  deadline. Resource caps limit their input and work instead.
- **DEBT-01:** AABB discrete physics, tunnelling at high velocities and the lack
  of a broad phase are engine-design limitations. Differential physics tests
  protect existing behavior; this patch does not replace the physics engine.
- **DEBT-02:** Merge/character generation does not preserve a full material,
  animation and property graph. Use copies of source objects when this data is
  needed. This requires a separately specified merge contract.
- **DEBT-03:** Duplicate object names remain allowed; `find_object` selects the
  first match. Use stable IDs for unambiguous MCP references.
- **DEBT-04:** Strict compiler warnings, vendored JSON headers and the ImGui demo
  are not demonstrated security vulnerabilities. Removing/changing them is
  deferred rather than weakening checks or changing UI APIs speculatively.
- **DEBT-09:** Extended-length Windows paths are not newly supported.

## Validation record

The Linux implementation passed the ordered checks below. Windows implementation
changes are compiled and header-checked; native Windows execution remains
**unverified** in this Linux environment. The local validation preceded publication of 1.2.2; the release pipeline adds native Windows checks before publishing.

1. **Build/syntax: pass.** Optimized `make -j2 engine engine-testing
   test-integration`; MinGW `make -f Makefile.win -j2 all tests`;
   `python3 tools/verify_windows.py build/win/engine.exe`; Python compilation,
   workflow YAML parsing, shell syntax, generated-manual consistency and
   `git diff --check`. ELF headers confirm PIE, RELRO and immediate binding;
   PE headers confirm DEP, ASLR and high-entropy ASLR.
2. **Security triggers: pass.** `python3 tools/test_audit_security.py ./engine`:
   caught instruction exhaustion, xpcall, new/retained/wrapped coroutines,
   finalizers, allocator exhaustion, excessive repetition and native pattern
   searches, malformed UTF-8 output, traversal/UNC/symlink escapes and unknown
   primitive names. The original infinite-loop examples now return budget
   errors; finalizers and prohibited filesystem operations are rejected.
3. **Legitimate controls: pass.** `python3 tools/test_studio_mcp.py ./engine`,
   integration tests, `make test test-rust test-kernels test-physics`,
   `python3 tools/test_studio_audio.py`, and the packaged-game test with
   `LUNORYX_TEST_RUNTIME=.../engine-testing LUNORYX_GAME_GUI=1` and software GL.
   The relocated game completes pickups, score, embedded audio and restart
   after its source files are removed. Project/legacy-array round trips,
   mirrored transforms, external-asset embedding, recovery snapshots,
   unchanged output on failed export and certificate-table package detection
   pass. The Khronos validator reports **0 errors, 0 warnings**.
4. **Sanitizers: pass.** The complete integration-test translation unit was
   separately built with `-O1 -g1 -fsanitize=address,undefined
   -fno-omit-frame-pointer`, linked against existing unchanged native support
   objects, and run with ASan leak detection and UBSan halt-on-error. No errors
   were reported. This focused run does not claim full instrumentation of the
   unchanged third-party libraries; CI retains its full sanitizer rebuild.
5. **Production/test separation: pass.** A production GUI process ignores
   `TEST_TIMEOUT`, `LUNORYX_UI_SMOKE` and `LUNORYX_CAPTURE_PATH`; the dedicated
   testing executable honors them and produces a capture. The production
   executable retains MCP for editor use; packaged player mode rejects it.

New regression sources are `tools/test_audit_security.py` and
`tests/audit_regressions.h`. Existing project/game/GLB/audio checks were updated
for base64 while preserving legacy-byte-array rejection tests.

A separate read-only reviewer identified the Windows icon temporary-file issue;
that path was repaired with exclusive temporary creation. Its remaining review
stopped because the provider applied a cybersecurity content filter. A local
caller/condition compatibility review was completed instead; a complete second
agent sign-off is not claimed. GitHub Actions, provenance publication and native
Windows runtime checks had not yet run at the time of this local validation. The published release report links its final CI run.
