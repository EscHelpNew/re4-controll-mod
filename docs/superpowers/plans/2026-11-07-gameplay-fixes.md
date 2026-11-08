# Gameplay regressions after console0.23

User evidence: every non-Leon appearance stays Pending in the in-game0.23
overlay; grenade explosion fog can leave the camera view; TMP firefights can
lose both right-stick and gyro aim, and releasing/re-aiming is not reliable.
Current0.24 weapon UI/pickup work is complete and frozen but not yet transferred.

1. Trace appearance request, renderer/mount/identity admission, load and draw
   acknowledgements. Preserve resource ownership and thread/GPU fences. Fix a
   proven blocker; otherwise expose distinct waiting reasons for a console probe.
2. Trace native TMP raise/fire state order and reproduce the simultaneous input
   latch failure through actual callbacks. Rearm only a proven ready native
   state while preserving reload, damage, menu and unsupported-handler gates.
3. Trace grenade marker clipping and add minimal camera-only framing around the
   actual cached explosion floor point. Preserve native eye/collision, throwing
   input/pose, trajectory, RNG and fuse. Make only the center redder and keep
   soft transparent outer edges.
4. Integrate targeted regression fixtures and guarded native sites. Localize
   appearance diagnostics without changing settings or shared layout if possible.
5. Run the required full host suite, ARM module/overlay/NRO builds, final review
   and source/history/frozen-artifact audit. Preserve frozen0.23/0.24 artifacts.
6. Deliver the paired build when the user's app receiver is available. Obtain
   console evidence for appearance and gameplay; do not claim host tests prove
   hardware visual/aiming correctness.
