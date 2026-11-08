# Native popup style and optional blade readiness

User0.26.6 feedback: skin selection shows120, weapon names now appear but their
typeface differs from the case, and fog is not visible. Actual paired0.26.6
snapshots prove startup guards pass, Active Krauser/Ashley occur, and numerous
selections stay Pending120 (WaitingSkin), rather than permanent load failures.

1. Remove the new optional-blade dependency from valid donor-body readiness.
   Capturing/linking a blade must not withhold body skin stamps or replace the
   entire donor body with Leon. Hidden blades do not need geometry copying.
   Preserve all visible-blade source, bounds, identity, capacity and slot-fence
   checks. Invalid blade data omits only the blade. Unsafe body identity or
   failed original native skin retains its existing fallback. Reproduce the
   dependency with actual callbacks, then verify body Active publication and
   visible ordinary/Krauser blades remain correct.
2. Replace popup Liberation artwork with the exact native inventory item-name
   font/style18x24 and palette0. Read the full localized name from bounded loaded
   MDT records; never guess Cyrillic glyph indices or fall back to another face.
   Reuse the existing native2B28E0 packet renderer. Preserve latest-event timing,
   right anchor and viewport compensation, with correct512x384/448 units.
3. Fog uses the already proven untextured triangle pipeline. Source-over gray155
   at peak alpha46 is invisible on a matching background. Use SrcAlpha/One for
   the fog only, retaining geometry, color, feather and envelope, then restore
   native state before drawing text. Test actual blend calls and glyph packets.
4. Add small optional source/blade-buffer observations to the existing read-only
   diagnostic packet so exact eligibility failures can be identified on hardware.
   Preserve96-read/16KiB bounds and independent validity, no process pointers.
5. Review, integrate guarded native font provenance, test/build/audit/freeze
   separately as0.26.7, transfer and confirm skin switching, both blade paths,
   case-identical captions and visible fog on the authorized console.

Accepted camera, global HUD, counter appearance, inventory drawing, SharedABI23,
wire19 and other mods remain unchanged. No commit/public release requested.
