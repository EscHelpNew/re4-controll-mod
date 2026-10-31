# RE6 profile: steady shoulder framing

The user accepts RE4 graphics and wants Leon to remain near a fixed side of the
frame during movement and turns, with less movement during aiming. The currently
successful aiming direction and sensitivity must remain unchanged.

The existing FinalLookAt owns direction, but the eye still inherits native
locomotion/weapon profiles and the smoothed17F40 basis. Correct the desired eye
before18990 collision, using actor-root translation and the owned heading/pitch.
Do not filter actor movement, change bones, redirect shots or override the final
post-collision eye. Collision may legitimately move the character within the frame.

Use the existing native neutral shoulder dimensions (eye0 -500,1765,-1190;
eye1 -180,1700,-110), preserving the distance slider1..2. Rotate the arm with the
owned analytic forward/up/right vectors around a root-height1700 pivot. Transform
the desired world eye through the inverse captured basis. Preserve native eye1,
roll and FOV; reconstruct the target along owned look. This removes dependence on
native profile blends and delayed basis translation without a stateful camera lag.

Only RE6-response unified third-person views of Leon use this rig. Default,
classic Custom, first person, other characters, scopes, scripted/reset cameras,
invalid values and unsupported actor states retain their existing paths. Invalid
rig calculations must not write partial output. No new hook, wire or ABI change.

Reload movement is already true in re6_profile(). Preserve that default and prove
that choosing RE6 from an old reload-off configuration enables it and saves it.
Do not silently replace explicit Custom choices or old settings during decoding.

Automated tests establish geometry and adapter gates, not comfort or wall quality.
Hardware checks must cover walk/strafe/run, turning, raise/fire/reload and walls.
