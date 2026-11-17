# Optional publisher signing — CI preparation

The application remains offline. Publisher signing/notarization is an optional **manual GitHub Actions** operation; no account, network client or credential is added to the editor or player. No publisher certificate was supplied during this implementation. Real Authenticode/Developer ID/notarization execution is not claimed tested.

1. Complete `Build and tests` for the exact current `main` commit.
2. If desired, configure credentials through repository **GitHub Secrets**, never in chat, source files, project files, artifacts or the HMAC package-key field.
3. Manually dispatch `Prepare optional platform signatures` (`signing.yml`) with `source_run` from that successful build. It refuses another commit/branch or unsuccessful jobs.
4. Publish with `release.yml`, supplying the original `source_run` and optional `signing_run`. The gate requires a successful signing run on the same commit and provenance bound to the same original source run. Leaving `signing_run` empty retains the normal unsigned/ad-hoc release path.

Windows secrets:

- `LUNORYX_WINDOWS_PFX_BASE64`: base64-encoded publisher PFX with one private signing identity.
- `LUNORYX_WINDOWS_PFX_PASSWORD`: its nonempty password.
- `LUNORYX_WINDOWS_TIMESTAMP_URL`: an HTTPS timestamp endpoint.

The Windows job uses the installed SDK signtool, signs owned editor/worker binaries, verifies Authenticode policy, signer identity and timestamp, then removes imported certificate/key material. Microsoft/third-party binaries are not re-signed as Lunoryx. Temporary credential files are excluded from artifacts.

macOS secrets:

- `LUNORYX_MACOS_P12_BASE64`, `LUNORYX_MACOS_P12_PASSWORD`.
- `LUNORYX_MACOS_SIGN_IDENTITY`: exact `Developer ID Application: ...` identity.
- `LUNORYX_NOTARY_KEY_BASE64`, `LUNORYX_NOTARY_KEY_ID`, `LUNORYX_NOTARY_ISSUER_ID`: Apple notary API credentials.

The Mac job uses a temporary keychain, signs nested Mach-O code and the bundle, submits a CI-only notarization request, requires `Accepted`, staples/validates the ticket and assesses Gatekeeper. Only the trusted C# worker receives JIT/native-library entitlements to preserve its existing full-OS-trust contract. The editor is not given those worker entitlements. Keychain/credential files are removed after the job. This path needs real native/certificate evidence before any positive publisher claim.

No credentials means `unsigned` on Windows and `ad_hoc` on macOS, with `publisher_verified=false`. Partial configuration fails; readiness alone is `configured_unverified`. `BUILD-SIGNING.json` records actual outcomes, `SIGNING-PROVENANCE.json` binds them to source/run, and SBOM/checksums are generated again after signing/stapling. The report rejects unbound or incomplete positive metadata.

The final uploaded release still has four assets: Windows ZIP, universal macOS ZIP, Verification.mp4 and Audit-report.html. Signing metadata, licenses, SBOM and checksums stay inside the ZIPs. The signing workflow itself prepares artifacts and does not publish a release automatically. HMAC package authentication is separate from publisher signing and does not replace it.

Build CI exercises the no-credential Windows branch and verifies the ad-hoc universal Mac bundle. Policy tests cover absent/partial/complete configuration and reject unbound/incomplete positive metadata. These checks do not validate real certificate import, timestamping, Developer ID signing, notarization or Gatekeeper acceptance. Generated games need their own publisher-signing process after packing; copying an editor certificate does not sign a modified executable.
