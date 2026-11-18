# Manual local updates and bounded application reports

The user requests a server explicitly launched from Updates rather than automatic network startup, and error reports downloadable while that server is enabled. Retain only the five newest reports and keep each below1MiB.

## Approach

Use an application-owned transfer-only receiver sharing the existing authenticated protocol and atomic NRO publication helpers. The legacy resident receiver cannot stop safely and accepts remote RUN; keep its early bridge compatibility but do not start it from the app. A full diagnostic/recovery-only launch is a temporary isolation tool, not a claimed permanent freeze fix.

## Behavior

Updates has a Local update page for RE4/RE5/RE6. Each application launch starts Off. Start initializes sockets and an interruptible worker only on explicit selection. Back/Stop/Exit requests cancellation, shows Stopping until the worker has exited, and closes network resources before returning. Stored means the receiver remains available for the second BRIDGE/GAME PUT. RUN is rejected; installation requires manual app close/reopen. GitHub networking/updater construction is also lazy, with exclusive socket ownership.

Record only bounded startup/action/error/exit stages, never per-frame telemetry or credentials/argv. Rotate five fixed own report files, each64KiB, total320KiB; missing storage never prevents startup. Download only these own files with authenticated CRASH, bounded below1MiB. Write before/after service boundaries so a stalled launch leaves the last completed operation. A hard lockup may prevent a final record; no claim that all crashes yield an exception report.

## Validation and limits

Host tests cover default Off, all game navigation, lifecycle/cancellation, AUTH/PUT/report/RUN rejection, atomic failure, five-file rotation and size limits. Build modules/overlays/NRO, bump0.28.14, run full suite/audit. Real HOME startup/sleep/exit stability needs console confirmation. RE4 numeric-HP absence remains separately unmeasured; do not fabricate a fallback or claim this server change fixes HP.
