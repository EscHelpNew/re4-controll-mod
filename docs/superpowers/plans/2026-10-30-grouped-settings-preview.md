# Grouped settings and reticle preview

The user requests a live picture of the reticle while editing, language selection
only on the main menu, and proper settings groups. A newly reported game crash
takes priority; no new console build is delivered until its evidence is examined.

- [x] Retrieve the actual crash report and identify its module/PC before changing
  native code. Preserve the delivered0.7.0 ELF and NRO for symbolication.
- [x] Add shared EN/RU groups: Sticks, Aiming, Gyro, Camera, Reticle, Mapping,
  Profiles. Every gameplay control belongs to exactly one group; Language is a
  root-only preference. Graphics remains a separate root section.
- [x] Update installer model and overlay navigation with tests for Back, immediate
  value updates, no writes on navigation, root language, profile and mapping actions.
- [x] Draw the selected dot/crosshair and size on a preview panel when Reticle or
  Reticle size is focused. Show Off explicitly. Use native geometry and a labelled
  magnification so the illustrated marker cannot be mistaken for an exact screenshot.
- [x] Verify actual SDL pixels and overlay draw calls for all styles/sizes, EN/RU,
  selection changes and absence of preview for unrelated controls.
- [x] Run tests, inspect previews, build, audit and deploy through the pinned
  library. Keep personal credentials private and the GitHub release hold active.

The settings wire and native behavior need no UI-related changes. Group
implementation and crash investigation have separate file ownership.
