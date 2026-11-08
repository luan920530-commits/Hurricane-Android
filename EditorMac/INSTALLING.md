# Installing the first macOS editor build

This preview build is for **Intel Macs (`x86_64`) running macOS 12 Monterey or later**. It is an early port of the Hurrican level editor, not yet feature-complete.

1. Download either `HurricanMapEditor-macOS12-Intel.dmg` or `HurricanMapEditor-macOS12-Intel.zip` from the GitHub Actions artifact.
2. Open the DMG, or unzip the ZIP, and move **Hurrican Map Editor.app** to Applications (or another folder you control).
3. The preview app is ad-hoc signed and not notarized. The first time macOS blocks it, Control-click/right-click the app, choose **Open**, and confirm. This one-time override may be required again after replacing the app with a newer build.
4. Open a game's `.map` file with the app. To preview the correct tile atlas, keep the map in its game data tree so the editor can locate the sibling/ancestor `textures` directory.

The app bundle contains the SDL runtime libraries used by the editor. It does not install or modify the game data. Back up maps before saving edits.

## Build provenance

The GitHub Action builds an Intel-only app with a macOS 12.0 deployment target, runs the `.map` format tests, bundles non-system SDL dependencies, and publishes ZIP and DMG artifacts with SHA-256 checksums. No Apple Developer signing identity or notarization credentials are used for this preview.
