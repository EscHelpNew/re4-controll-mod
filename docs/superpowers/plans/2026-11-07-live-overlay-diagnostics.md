# Live RE4 overlay diagnostics

The user explicitly requested a port in the overlay so the agent can collect
errors directly. This extends the owned-hands feedback candidate into0.26.0.

Use authenticated TCP28773 with the existing local connection key. A dedicated
64KiB worker remains active while Tesla hides this overlay; leaving/unloading
the overlay stops the worker before BSD shutdown and SD unmount. Missing keys,
SM/BSD failures and bind/thread failures leave normal controls usable. The main
menu shows port state. Credentials remain local and never enter reports/assets.

The protocol accepts exactly AUTH and SNAPSHOT. Replies are schema1 JSON, capped
at16KiB. Lines, partial I/O, cancellation and an8s connection deadline are bounded.
There are no arbitrary memory, file-path, write or execution requests. The worker
owns a separate GameLink/service; it never shares the UI's mutable link and never
calls settings application. Supported title/build and process identity precede
fixed native reads. Missing games and unsupported builds produce status reports.

Snapshots report actual module/overlay version, controls/sensor state, settings
validity, appearance errors/loading progress, popup producer/HUD counters, cached
viewport/scissor and optional player/camera observations. NaN values become null;
addresses, files and keys are omitted. Independent settings/appearance/GPU
sequences retain subsection validity without discarding other useful data.

SharedABI23 appends ordered popup capture and a module version string, preserving
all previous field offsets and settings wire18. An owned native command after
the popup text End records viewport/scissor at that CPU render-command boundary.
It changes no renderer state, adds no hook and does not sample raster output.
Zero/negative finite bounds remain observable with rendering usability separate.

The host client reads the existing local key without putting it in argv, fetches
one snapshot or polls into exclusive JSONL output, and never echoes secret-bearing
network errors. Files remain in ignored build/diagnostics by default workflow.

Tests cover protocol framing/auth/limits/loopback/timeouts/fixed-port restart,
worker service/socket failures and teardown order, JSON grammar and escaping,
supported process checks and capture races, native ordered command execution,
host framing/token/output handling, and existing overlay controls. Run the full
suite, ARM modules/overlays/NRO, profile-word checks, independent review and
public/history/frozen-artifact audits. Then transfer the paired candidate, have
the user install/restart the game and load/hide the overlay, and verify an actual
authenticated console snapshot before claiming that the port works on hardware.

Owned-hand, knife and native-caption changes from the previous feedback plan are
included. The accepted grenade camera stays unchanged. Popup visibility remains
unresolved until live observations identify the presentation failure. No commit,
push or public release is requested.


##0.26.1 console startup regression

Automated support reports, fetched through the existing application log service, prove0.26.0 SharedABI23/status2/frames0 with correct paired file identities. New knife guards included4 raw words spanning2 loader-relative pointer slots. Authenticated ELF dynamic records establish R_AARCH64_RELATIVE type1027. Replace these raw comparisons with exact runtime base+addend checks; retain all instruction/build/bounds guards. Public literal runtime fixture REDexit3 beforefix, GREENexit0 afterfix; entrypoint rejects corrupted pointers before hook installation. Private authenticated full-image simulation applies15711 relocations and passes1428 guarded words plus2 exact pointers; no remaining guarded word intersects a relocation. Independent review has no Critical/Important finding. Complete fullsuite, ARM/overlay/NRO, packaging/audit, then deliver paired0.26.1. Hardware recovery and TCP still pending. Accepted grenade camera untouched.


##0.26.2 transport failure observations

Console0.26.1 user confirms visible RE4 Controls overlay, port28773 remains shown. TCP connections succeed then reset within3..50ms even with no AUTH bytes; full and partial AUTH behave identically. Game recovery is still unverified. No transport rootcause proven. Missing time-service hypothesis was rejected: shipped overlay has zero BSS boot-time values and no initializer call, so monotonic syscall can use uptime. Do not change game hooks, compatibility guards, camera or TCP buffer/service settings speculatively.

Implement minimal failure observations: record first terminal transport stage, descriptor, return code, errno, native BSD Result, elapsed monotonic ticks. Capture immediately around accepted F_GETFL/F_SETFL, read/write select and recv/send, cancellation. Preserve first error. Offer a bounded nonblocking generic ERR TRANSPORT line when feasible before close; no game data before AUTH. Optional worker-owned callback writes a bounded fixed-path transport log; app support report includes it. Desktop client strictly parses only numeric diagnostic fields, never unauthenticated arbitrary text, and saves safe fields in watch errors. Test injected adapter failures with real loopback networking, client error parsing, and callback logging before implementation; no guessed fix. Then full host suite, ARM/payload builds, peer review, pairing/artifact audit and a local frozen0.26.2 for the next console probe. Actual gameplay remains unchanged except its version label. No commit/public release.

File ownership: transport header/server/native fault fixtures in the server task; Python client/tests, frontend callback/log integration, support-report allowlist, version/build registration/docs in root. Shared API fixed before parallel work.

Shared-worktree integration: user confirms parallel extra-weapon-upgrade/case-ammo task. Preserve shared sources and pairedWire19 (bothnewoptionsdefaultfalse). Budget60/61,1474guards+2exactpointers pass actual relocatedimage. FirstfullsuitePASS69Python/allnative. Independent integrationreview identified sidecarloadauthority andwrite-retry loss paths; repair them with actualcallback/SDK-failurefixtures before freezecandidate. Preserve unresolved loadedslot/CRC until maskrecovered; prevent unknownownership purchases/checkpoints/Save. Successful flushed sidecarwrite commits candidatecache; failedwrite keeps priorcache and retries. Physicalwholefile-write atomicity remains outside this minimal correction. Re-runfullsuite/moduleafterfix andchecksourcecoherence withuserparallelwork.
