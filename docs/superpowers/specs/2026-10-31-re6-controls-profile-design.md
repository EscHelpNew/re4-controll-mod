# RE6 controls profile for RE4

The user now explicitly wants RE6's different overview and aiming speeds,
superseding the earlier request for one identical speed in both modes. Build a
separate RE6 controls preset, available beside Default, RE9 and Custom. Existing
profiles retain their behavior. No additional user recordings are required for
this first approximation.

Evidence: one verified console journal, 1437 synchronized samples in40.7329s,
zero lost/invalid intervals. Overview yaw171.6degrees/s and aiming yaw57.3degrees/s
are observed sustained rates. Two release traces independently fit47ms decay.
Aiming pitch is provisionally53degrees/s from shorter50.9/54.1degrees/s traces;
overview pitch remains90degrees/s because it was not isolated in the recording.
Rise response is approximated from the same existing input/angle traces and
documented as a fit, not a complete reproduction of RE6's camera/animation engine.

Approach: preserve the proven native RE4 adapter and stable camera; introduce a
persisted RE6 response flag that selects per-route rates/timing in UnifiedLook.
Walking uses Camera gain; aiming/knife uses RightAimX/Y gains. Gains remain0..1,
deadzone .05; gyro .8 aim-only, inversions on, existing movement and button-layout
choices preserved. Input/weapon/route transitions drain stale velocity and do
not replay weapon-raise input. Selecting or editing a preset preserves graphics,
language, calibration and saved Custom snapshots.

Use settings wire12 (bit4 byte97), migrate1..11 with the flag off. Shared ABI16
keeps the same400-byte layout and rejects older adapters explicitly. A revised
module and overlay must be installed together through the own installer. Keep
all files here, tests in CI, credentials/logs out of Git, and no GitHub release/tag.

Hardware verification compares the new preset in RE4; actual RE4 engine camera
behavior remains a validation step after the first reference-based build.
