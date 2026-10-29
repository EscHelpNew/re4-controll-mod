# RE4 Controls: language, curves and installer UI

The user's2026-10-29 request extends the working0.2.2 mod. Work remains in the
current repository and uses the existing library for all NRO transfers.

- English by default; English/Russian selectable and persisted, including help.
- Remove the walking-speed setting from both interfaces; proportional walking
  remains and its full-stick maximum stays the native speed.
- Default both deadzones0.05 and both gyro inversions on.
- Select one common stick-response curve: linear, exponential, logarithmic,
  or exponential-to-logarithmic with a smooth transition. Linear remains default.
- Keep stick/movement gains capped at1.00. Curves preserve zero, sign, full
  scale and monotonicity; no gyro response-curve transformation.
- Replace the diagnostic console's main screen with a themed installer using
  the supplied red4 logo, dark background, muted red highlights and warm text.
  Put raw technical readings on a separate diagnostics page.
- Use the same bilingual control metadata in installer and overlay, with
  immediate language updates and descriptions for every focused control.

Implementation choices made by the agent: use installed SDL2 for the standalone
NRO renderer, existing shared system font and vendored stb_truetype for Cyrillic.
SDL2 exists for both Switch and Windows, enabling actual renderer previews.
Keep the existing install guards, token handling and receiver polling lifecycle.
Retain install shortcuts R/ZR/MINUS on the installer main page.

Settings remain64bytes. New version4 uses previously unused flag bits for
language and curve; old files decode with English/linear and native walking
maximum. Bump shared ABI so old game modules cannot silently ignore new settings.
No old presets are overwritten until successful save.

Tests cover response math, migrations/defaults, shared menu metadata, the actual
overlay callbacks/IPC, installer navigation and actual rendered EN/RU previews.
Hardware acceptance covers installation UI, both languages and game curve feel.
