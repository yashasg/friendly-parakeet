# macOS CI Signing and Notarization

Issue: #187

`ci-macos.yml` includes a dedicated `notarize` job that runs only on:

- `workflow_dispatch`
- tag pushes matching `v*`

The job downloads the built macOS binary artifact, imports an Apple certificate into a temporary keychain, creates and signs `SHAPESHIFTER.app`, submits it to Apple notarization using `xcrun notarytool submit --wait`, staples the notarization ticket, and uploads the notarized `.app` plus zip.

## Required GitHub Secrets

- `APPLE_CERTIFICATE` — base64-encoded `.p12` certificate (Developer ID Application or Apple Distribution)
- `APPLE_CERTIFICATE_PASSWORD` — password for the `.p12` certificate
- `APPLE_ID` — Apple account email used for notarization
- `APPLE_TEAM_ID` — Apple Developer Team ID
- `NOTARIZATION_PASSWORD` — app-specific password for notarization
