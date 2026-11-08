# Live gameplay feedback correction plan

Continue the user's RE4 Controls debugging through the existing Uberhand overlay.
Keep accepted grenade framing, parallel upgrade/ammo features, settings wire19,
SharedABI23/488 and the working0.26.3 TCP/shared-service implementation.

Hardware evidence:0.26.3 module status1; requestedKrauser4 fails with reason10,
stage23 unique_count observed15. Native23D010/14 mutates reference DDS IDs into
cached descriptor IDs. Krauser's authenticated union10 has5 repeated shared-TPL
references, exactly reproducing the false count15. Collect the complete original
unique ID/ref/creator-argument set before any creator call; validate the exact set,
then create once per entry. Retain capacity, ownership, phase and upload fences.
The actual callback fixture must fail before the correction, then pass mutation,
alias, wrong-set and cancellation cases afterward.

Popup evidence:P3/H5/events6/frames255, ordered capture sequence578/status4.
Native64E084..B0 deliberately stores disabled scissor0,0,INT_MAX,INT_MAX. Accept
only that exact additional class in module capture and overlay observation/JSON,
preserve raw integers and mark scissor_disabled. Reject near misses and retain
finite viewport/stale checks. This is a diagnostic correction; missing raster
text/mist is not yet explained. Preserve drawing state, placement and timing.

Integrate20 authenticated native words, revalidate the relocated image, review
both deltas, run the full suite and ARM/module/overlay/NRO builds, verify staged
and embedded pairing, audit public source/history/artifacts, preserve old builds,
freeze a local0.26.4 and transfer to the authorized console. After paired install,
collect real appearance and viewport/scissor observations for further debugging.
No public release or commit is requested.

Observe TMP during ordinary play. A hidden-overlay recording has220/220 replies,
but the user's subsequent shot sequence fell outside that short interval. No
claim of complete TMP recovery follows from those counters alone.

User added a precise visual change during verification: remove the spare-ammo
icon; reserve the original three-digit number field and center actual quantities
inside it without leading zeros. Keep the existing baseline, size, color, fade
and inventory semantics. Quantities wider than three digits remain complete
(up to native u16), centered on the same point. Actual HUD fixture first fails
on icon-inclusive count, then passes icon-free geometry and shared center for
8/88/888. Rebuild paired payloads and rerun final checks before transfer.

User also reports missing knives on new skins. Current installed0.26.3 has a
confirmed texture-reservation failure; do not infer knife binding correctness
from host tests. Recheck fully active appearance on hardware after0.26.4.
