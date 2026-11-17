# Local package authentication — 1.4.0 development

New games use `LNXPCK02` containers authenticated with HMAC-SHA-256. This is development work for 1.4.0, not a claim about published 1.3.1 binaries. No new cryptographic dependency or network service is used.

Create a **new** external key file once:

```sh
./engine --generate-package-key /private/location/game.key
```

The command uses the OS random provider, writes exactly 32 raw bytes, sets owner-only permissions (owner/System/Administrators on Windows), and refuses to overwrite an existing file. Keep it outside project folders, `Runtime/`, app bundles and release archives. Do not commit it. Preserve a secure offline backup: losing it prevents authentication of games built with it.

Select **Game / Build → External package key file** or supply the path explicitly:

```sh
./engine --package-key-file /private/location/game.key --build-game project.lunoryx Game
./Game --package-key-file /private/location/game.key
./Game --package-key-file /private/location/game.key --verify-package
./engine --package-key-file /private/location/game.key --verify-package Game
```

`LUNORYX_PACKAGE_KEY_FILE` provides the same local configuration. An explicit `--package-key-file` overrides it. Key selection is local process configuration, not project data; it is not serialized into the project, copied into the game, or exposed through a Lua/MCP API. Arbitrary trusted C# retains user OS permissions and can access user-readable files; this is not an OS secret-isolation guarantee.

On Windows use `Lunoryx.exe`/`Game.exe`. For a Mac game pass the option to `Game.app/Contents/MacOS/Game`, or launch it with the environment variable configured. Linux/macOS input keys must be owner-only regular files (`chmod 600`); normal editor/MCP use requires no package key when no package is being loaded or built.

The verifier authenticates a fixed domain, every parsing-critical footer field, the manifest-length prefix, manifest and complete payload. It uses the captured resource bytes for both HMAC and the committed assets. Bounded manifest validation can reject malformed input before tag comparison; no project/managed extraction/script startup occurs until the authenticated asset transaction succeeds. Wrong/missing/malformed keys, failed tags and CRC-only `LNXPCK01` packages are rejected without replacing the current result. Old editable projects remain a separate migration concern; rebuild old games from their project using the new runtime and key.

HMAC is a **symmetric MAC**, not a public digital signature. Every verifier that possesses the secret can also forge packages. It does not encrypt content, authenticate the native runtime/publisher, or prevent replay of an older valid package. Windows Authenticode and Apple Developer ID/notarization are separate CI tasks and must not be claimed without real credentials/evidence.

Existing limits remain: 1 GiB payload, 128 MiB runtime template, 4096 total resources and 4 MiB manifest. CRC32 remains only in unrelated deduplication buckets followed by byte comparison. Budget tests include sparse authenticated containers at 1 GiB−1, exactly 1 GiB and rejection above the cap.

The active key is protected against editor writes/removal as well as game-output aliases. Runtime/resource collection rejects the key file; the serialized output is scanned across copy/resource boundaries for its raw bytes. Background asset jobs finish before key/I/O state is destroyed. This does not prevent a user or trusted OS-level C# code from deliberately exporting secrets in another representation.
