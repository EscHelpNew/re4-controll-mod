# RE6 measurement page and telemetry

The user requests a second installer page selected with L/R. RE4 retains its
existing settings. RE6 has a violet crosshair/6 emblem and exactly four home
actions: install RE6 module, install RE6 overlay, language, exit. Language is
shared. Switching games returns to that game's home and never installs anything.
The existing HOME shortcut remains RE4 Controls.

RE6 is a measurement reference for later RE4 tuning. Keep its original controls,
camera, weapons and animations. Record synchronized raw left/right sticks,
buttons, player position/orientation and camera eye/view orientation with frame,
monotonic timestamp, session/build identity and explicit validity/drop counters.
Preserve raw values and units; derive displacement/angular velocity offline.

Local sources: base title01002A000CD48000 and update131072. Installed version is
unknown. Confirm the program Build ID from extracted ExeFS and the console;
never apply guessed offsets or claim success with an empty payload. Unknown
builds must report unsupported rather than install active hooks blindly.

Use existing native module and overlay infrastructure where it fits, with RE6
receipts, paths, shared marker, profile and overlay separated from RE4. No writes
to game state except verified observation hooks. Capture must be bounded and
avoid synchronous filesystem writes in frame callbacks. The chosen design is a
native capture ring plus an overlay worker that persists records while hidden.
The user opens that overlay once per session; replacing/closing it stops its
worker, which must flush/join before services close. Report failures, inactive
collection and actual sequence gaps explicitly. This avoids an unverified
thread/filesystem ABI inside the game.
Expose measurement validity honestly when actors/cameras are absent or loading.
Logs stay on SD and ignored local data; no game material or credentials enter
Git, packaged assets or reports.

Implement page/model/renderer independently from native address research. Native
telemetry installation is enabled only after its actual payload/profile exists.
The user confirmed the download complete; hash-checked local extraction resumed.
Follow tests-first, full build, independent review and secret audit. Source main
push remains authorized, but GitHub release/tag hold remains in force. Console
NRO delivery uses the pinned switch-monitor client exclusively.
