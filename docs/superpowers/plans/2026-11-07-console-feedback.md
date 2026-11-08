# Console feedback after0.24.1

Evidence: the console reports appearance wait109; grenade aiming twitches and
the marker still clips; D-pad changes produce no name/mist; the photographed
inventory firepower line is too large and unlike native text.

1. Trace109 through the actual native effect owner/consumer and narrowly admit
   the normal constructor-installed representation, retaining unknown-owner gates.
2. Remove prediction-dependent camera rotation. Test the real native grenade
   camera profile2 and current projection/follow order, then smoothly widen the
   lens while held. Preserve aim/eye/collision/physics. Clip a real partially
   visible marker truthfully instead of discarding the entire mesh at one edge.
3. Move weapon feedback after native HUD packets and add a real-ID end-to-end
   D-pad transaction→mailbox→timed HUD regression. Do not claim native opaque
   overdraw is the sole observed console cause before validation.
4. Replace the custom inventory damage font with compact numeric glyphs from
   the live native name font/color/outline. Read current locale font and glyph
   metrics; do not guess Cyrillic codes or modify native message channels.
5. Run focused actual callbacks, the full host suite, ARM builds, fresh review
   and public/history/frozen-artifact audit. Freeze0.24.2 and transfer when ready.
6. Verify appearance/application and grenade/HUD behavior on the console. Keep
   previous frozen builds and unrelated features intact; no commit/publish.
