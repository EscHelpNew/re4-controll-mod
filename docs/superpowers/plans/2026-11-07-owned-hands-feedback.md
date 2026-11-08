# Owned hands and presentation feedback

Hardware0.25.0: menu appearance is correct; grenade camera is accepted and must
remain unchanged. Gameplay reports122 for all appearances. Inventory flash
grenade lacks its right hand, and appearances retain Leon gloves. D-pad popup
diagnostics are producer3/HUD5,21events/255render frames: publication and producer
geometry generation work, but visible presentation remains unresolved. Inventory
owned-font prefix IS visible; generic triangle rendering is therefore functional.

## Design

Complete each authenticated donor recipe with its own ordinary native hand
pieces, retaining existing bodies/head/accessories. Human Ramon already includes
complete hands and receives no duplicate pieces. Explicit piece roles distinguish
body, right hand, left hand and knife. Use the proven donor default helper
selection; weapon-specific finger gestures are not inferred from asset names.
Deform hands through the owned retarget palette with native wrist/grip anchors.
No native gameplay bones, weapon hit origins or action timing are changed.

With a complete owned body, gameplay and menu rendering no longer borrow native
hand-chain suffixes. This removes the fragile membership dependency that reports
122 and the incomplete inventory hand classifier. Keep strict supported owner,
asset, bone, material, effect, renderer and buffer lifetime checks. The native
weapon objects remain separate from the cosmetic body.

Krauser's blade uses the exact native standalone Leon knife render owner in the
existing draw hook. Omit it from body/menu chains. Validate native knife identity,
parent and root connection; retain native visibility, lighting and effects. Its
owned blade is skinned in player space, with the inverse knife-root/player-root
transform copied into native commands synchronously. Reject singular/unknown
owners and keep native fallback. Add rotated/translated matrix fixtures and exact
native instruction guards.

The accepted grenade lens/prediction and existing menu lifetime design stay
unchanged. Additional hand geometry must fit disjoint existing three-slot storage
in both gameplay and menu, and all additional texture IDs must be reserved/pinned.

For the inventory prefix, investigate a guarded native glyph mapping from the
loaded localized item-name bank. Four known Russian weapon names provide repeated
letter constraints; reject unsupported or ambiguous layouts before emitting
native packets. Do not guess Cyrillic atlas indices. Native digits remain21px.

The popup problem needs another presentation observation. Inventory visibility
rules out a generic0x90 primitive failure. Read the renderer's cached GPU
viewport/scissor state through the authenticated overlay debug connection. This
asynchronous paused-state reading is a bounded first observation, not a capture
synchronized to popup command execution; it cannot establish clipping at emission
time. Retain current animation and approved artwork. Do not label a speculative
draw-order/primitive change a confirmed fix.

## Validation

RED/GREEN actual callback fixtures cover complete owned hands with stale/cyclic
native hand caches, inventory flash right-hand geometry, separate native knife
replacement, exact native-owner rejection, transformed matrix order and slot
acknowledgements. Authenticate all seven archives and measure final geometry and
texture capacities. Test native glyph inference, unsupported translation
rejection and exact prefix/digit font/color/size equality.

Run full build.py tests, ARM module, overlays/NRO, profile-word verification,
independent resource-lifetime/presentation review and public/history/artifact
audits. Freeze a new candidate independently of0.25.0, then transfer when the app
is open. Console presentation and natural finger/weapon alignment require user
validation. No commit, push or public release is part of this iteration.
