# Character rotation in the attache case

The user requests horizontal right-stick rotation of the character preview.
Apply a proportional yaw to the exact native Leon preview model, covering both
Leon and its cosmetic replacements; leave the yaw at its current position when
the stick is released. Use the existing right-stick deadzone/curve, with a full
deflection speed of120degrees/second. That speed is an implementation choice.

Reuse the existing ModernRoot callback before original2B7C60, where the actual
case body update runs. The original evaluates motion and reconstructs the actor
matrix from its B0/B4/B8 rotation at2B87EC..18, so a B4 yaw update reaches the
whole native preview and the already implemented cosmetic rendering.

Alternative: create a separate menu-update hook and duplicate model integration.
Reusing the established callback avoids another hook and repeated updates.
Changing a draw matrix would require temporary native ownership and consistency
with skinning and held models; the native model update already handles these.

Admit only the authenticated menu pool/body identity. Read the existing atomic
physical input snapshot with its100ms freshness bound. Apply at most once per
simulation frame, use bounded wall-clock elapsed time, reset on changed preview
identity/renderer/menu generation or a long pause, and reject invalid yaw.
Gameplay actors, unrelated item previews and all input/button state stay intact.
No settings, ABI or menu option is added. This is the direct requested behavior.

Verify the real callback with a3C8-byte model: native sees the new yaw, both
directions, deadzone, full/partial deflection, release, duplicate calls, frame
rate independence, stale/disconnected input, paused clock, invalid identity,
new generation and simultaneous left-stick input. Existing appearance/knife
and gameplay movement/aim fixtures must continue to pass.
