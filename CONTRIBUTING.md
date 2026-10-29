# Contributing

Open an issue with the game Build ID, mod version, controller type and a short
reproduction sequence. Describe what moved and what you expected. Do not attach
keys, tokens, game binaries or private crash reports.

Keep changes in the root checkout. Add tests for changed behavior and run
`python build.py --test`. Game changes also require a module/overlay/NRO build.
Use actual callback/SDK-adapter fixtures when changing hooks or installation.

Public files must pass `python tools/audit_public.py --history`. Keep `project.json`,
`data/`, `build/` and credentials out of Git. Runtime secrets belong on the local
device or computer, never in examples or issue attachments.

Preserve the installed game, other mods and existing overlay loader. An unsupported
game profile must leave hooks disabled. New hardware claims need actual console
verification; host fixtures do not prove every game state.
