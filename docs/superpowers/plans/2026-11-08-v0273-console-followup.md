# 0.27.3 console feedback implementation plan

> **For agentic workers:** Use superpowers:subagent-driven-development for the independent investigations and superpowers:systematic-debugging before every fix.

**Goal:** Correct knife trails, natural-body weapon grips, actual projectile and TMP shell origins, and health-HUD disappearance during inventory transitions.

**Architecture:** Keep immutable native actor/skeleton data and ordinary weapon dimensions. Follow authenticated producer/consumer data flow and native render ordering, with bounded owned pose corrections and native fallback. Each console symptom must first have a failing actual-callback regression; existing caller-table tests alone do not establish coverage of real firing routes.

**Tech stack:** C++17/20 host fixtures, ARM64 game module, Python 3.13 build/coverage runner, authenticated read-only console diagnostics.

**Spec:** The user's five console observations after 0.27.2, plus the established ordinary-size-weapon and natural-limb requirements.

## Global constraints

- Preserve published/frozen 0.27.2 and commit/tag b940ece/v0.27.2; changed fix binaries use 0.27.3.
- Preserve native input, spread/direction, resource ownership and lifetimes; no blanket generic projectile/model-spawn translation.
- Keep normal mode free of automatic diagnostic logs and the overlay diagnostic server; console capture remains opt-in.
- Run meaningful focused RED/GREEN fixtures, then the complete one-command suite with 2–4 workers and at least90% executable-line coverage separately for C++ and Python.
- Build paired module/overlay/NRO; inspect any new native callback ABI and audit native guards, public source/history and packaged artifacts before delivery.

## Review focus

- Automatic first/subsequent shots and every supported weapon family must share the displayed ray's origin and actual collision path.
- TMP shell creation must consume the corrected world position while live fire updates native model state.
- Knife trail endpoint and history must follow the posed owned blade, including Krauser reversal and short characters.
- Both hands must contact plausible weapon grips while natural arm lengths and ordinary weapon dimensions remain intact.
- Native black fades must cover name/ammunition in the same compositing pass as the original health gauge; no late repaint.

## Independent tasks

- [x] Ballistics and TMP shells: aim_extremes_native owns authenticated firing/ejection census, character_emitter.hpp policy and focused ballistic fixtures. Root merges native guard and module-adapter changes.
- [x] Knife trails and grips: natural_height_research owns character_appearance.inc and focused owned-pose/grip helpers/fixtures. Establish the grip strategy from native geometry/attachment evidence before implementation.
- [x] Inventory transition HUD: ammo_health_visibility owns counter/name fixtures and native render-order evidence. Root moves only these two draws from HudDraw to the end of SceneDraw after a confirmed RED.
- [x] Integration: root owns version, module adapter/stubs, native profile guards, final review and matched builds. Coordinate shared fixture edits and keep compiler concurrency within2–4.
- [x] Validation: freeze source inputs, run complete measured acceptance and preserve exact artifacts.
- [ ] Console delivery and acceptance of the local candidate.

## Confirmed initial findings

- Native TMP handler328190 has separate BODY10 origin transforms328284/3283B8 feeding33B770; both are absent from the old ordinary-pistol-only shot whitelist.
- The TMP shell call3483AC is already listed. Its remaining failure needs pointer/timing/ownership investigation rather than another guessed call site.
- Native fullscreen fades215290 run after SceneDraw and before late29CEF0. Drawing custom gauge additions after29CEF0 necessarily places them above the completed black fade. Their draw order must follow the native gauge before that fade.

## Frozen candidate

- Registered109 native fixtures, including shared contact fitting, blade-tip geometry, actual body/held/emitter adapters, ballistic producers and ribbon seed/update regressions.
- Shared two-hand fitting requires the supported primary weapon; an auxiliary knife cannot authorize unsupported primary geometry. Native limb lengths and weapon dimensions remain intact.
- Knife samples use the exact posed blade geometry. Visibility changes preserve qualified ribbon history; a competing independent knife takes priority only while visible.
- Runtime profile audit confirms6431 raw guards and209 exact relocated pointers, without intersections with16654 native relocations. The authenticated relocated image and pointer-corruption negatives pass.
- Paired ARM module, overlay and application builds pass. Independent linked-code review confirms original LR, argument preservation, native fast paths and one original call per adapter.
- Full measured acceptance passes from333 frozen inputs with4 workers:110/110 top-level cases, including175 Python tests. Executable-line coverage is91.30% C++ and96.18% Python; all139 own C++ files are accounted for.
- Candidate0.27.3 is frozen separately. NRO SHA256 is94f3dc853f29a517859e4713ff5775d502d2cbb210776bbcb59c7914f03a1b5c; paired embedded payloads and previous frozen builds were verified. Console delivery and acceptance remain pending.
