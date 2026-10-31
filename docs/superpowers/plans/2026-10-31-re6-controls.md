# RE6 graphics overlay

The user requests a separate RE6 overlay and graphics settings. After finding
the native motion aiming menu, the user explicitly cancelled all RE6 gyro
changes. Retain native gyro and controls. Extend the existing RE6 observer
without replacing the accepted RE4 RE6 response preset or measurement overlay.

Use the current C++/exlaunch/libtesla implementation and verified RE6 update
v131072, SDK6.6 and its exact Build ID. Keep all work in this repository. No
GitHub release or tag; no private keys, tokens, game images or logs in Git.

Keep English/Russian labels and focused help, with a Graphics group and a
root-only language setting. RE6 configuration must be separate from RE4
configuration and must never alter RE4 presets. No RE6 gyro controls or hooks.

Graphics defaults must retain native rendering. Add only options backed by
verified configuration fields or engine calls; unsupported items must not
pretend to apply. Research texture filtering, anti-aliasing, draw distance and
shadows independently. A feature remains incomplete until its real runtime
integration, tests and hardware behavior are checked.

- [x] Cancel RE6 gyro modifications at the user's explicit request.
- [x] Verify safe graphics options and original/default values.
- [x] TDD separate bounded RE6 settings and persistence/migration.
- [x] TDD module settings startup, live updates and graphics scope.
- [x] Add dedicated RE6 overlay and installer payload with native defaults.
- [x] Run host/ARM/UI checks, independent review and secret audit.
- [ ] Merge tested source to main, verify CI, deploy via pinned library.
- [ ] Obtain console acceptance of each implemented graphics option.

RE4 version0.13 polish is validated and delivered separately while native RE6
research proceeds, so the existing requested fixes do not depend on new offsets.
