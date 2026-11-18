# Resident Evil 5 controls — 0.28.28 candidate

RE Controls supports the original Nintendo Switch RE5 port with independent
controls for both local players and shared graphics. The installer selects
**RE4 / RE5** with **L/R**; RE5 has the gray crosshair5 page. English and Russian
are available. [Русские примечания к релизу](releases/v0.28.27.md).

## Supported images

Title ID: `010018100CD46000`.

| Update | Main Build ID (zero-padded to 32 bytes) | Support |
|---|---|---|
| v196608 | `4E46401BF2FE53A44B69CC07FD951FE6` | Exact native profile; host regression coverage |
| v65536 / v131072 | `C517ECBB79DE97338E98147A6B5B5F2B` | Same older main; host coverage and accepted v0.28.27 console test |

Each profile validates its own GNU note and 2529 protected immutable words before
installing 52 hooks. Both require the same exact SDK identity and 30 protected
SDK words. Unknown or conflicting images remain inactive. The installed older
main and translation are preserved; the installer does not replace game archives.

## Install and update

1. Update the app using `re4-controls.nro`, or the in-app GitHub update. For a
   manual app update replace `re4-controls.nro`, `bridge.nro` and `game.nro` from
   the same release; the latter two are in `re4-controls-sd.zip`.
2. Fully close RE5 through **HOME → X → Close**. Open RE Controls, select **RE5**
   with **L/R**, then **Install / update mod**.
3. Restart RE5. In Uberhand/Tesla select **RE5 Controls**, then **Connect to RE5**.
   Return to the overlay list and select RE5 Controls again after an overlay
   update to unload any cached older copy. Check version **0.28.28**.

Install the full module and matching overlay together. Updating only the overlay
cannot supply new native camera/movement code or authenticate an old guard count.

The installer owns only `subsdk9` and `main.npdm` under the RE5 title and
`switch/.overlays/re5-controls.ovl`. It rejects foreign-file conflicts and rolls
back a failed transaction. Disable preserves RE4/RE6 files and RE5 configuration
at `sdmc:/config/re4-controls/re5-settings.bin`. Keep a configuration backup.

## Controls and camera

Select **Local player 1/2** before editing controls, opening Controller test or
calibrating gyro. Controls, gyro biases and Custom profiles are independent;
graphics are shared. AI and network actors do not receive custom local controls.

**Default** restores native controls with custom gyro/remapping off. **Modern**
uses the shared measured-response approximation, unified sticks and moving
aim/knife/reload options. The left stick moves; the right stick turns the
character and camera together. The overview camera follows the current player
anchor and supports vertical look within ±35°. Native wall collision retains
control near obstacles. Custom preserves manual controls; changing a controls
profile preserves graphics and gyro calibration.

Version 0.28.27 corrects the reversed Modern view direction and false camera
rejections when native movement/ground resolution changes position. Camera and
body retain one committed heading while the anchor follows the current native
position. This restores useful vertical look and stable ordinary framing in the
accepted test. Native desired/history smoothing and collision remain active in 27. A later
walking and standing-turn test reported framing drift. Candidate 0.28.28 changes
only the qualified native spatial-follow retention for a fresh Modern overview;
native FOV/roll, transition ownership, coefficient bookkeeping and collision
remain. Physical verification of this candidate is pending.

Optional movement while aiming/shooting, using the knife or reloading retains
native action timing, movement collision, ground stepping and leg IK. Ordinary
supported actions use qualified native weapon/animation data. Scripted turns,
cover, injury/death, frozen/pending actors, unsupported resources and scopes keep
native ownership. Scoped aiming stays stationary: right stick/gyro aim and left
stick changes zoom. Native weapon limits remain.

## Controller and gyro test

Controller test shows configured stick deadzones. **ZL**, or the Gyro group's
Debug entry, opens the static RE5 scene with the red point and gyro settings.
**X** calibrates a stationary controller, **Y** centers the point, **B** cancels,
and **Save and apply** saves the selected player's draft and returns to the
originating page. Edits affect the preview until saved; a failed save keeps the
active configuration. This is an application preview, not live gameplay. The
Tesla overlay has gyro settings; calibration/fullscreen testing is in the app.

## Graphics

Default keeps native graphics. Quality selects **AF16x**, **SMAA1X** and **1.75x**
matched projection/frustum far clipping, with native shadows. Custom stores manual
choices. This does not increase texture resolution or promise extra streamed
LODs. Point/comparison samplers stay native. Shadow Off affects only supported
stage-shadow passes. Fully close and restart RE5 after graphics changes;
filtering is latched when native samplers are created.

## Validation and known limitations

An initial user test accepted **v0.28.27** camera and all requested combat movement on the
installed older main, attached Joy-Con and **local player 1 (internal slot 0)**:
Modern turns/vertical right-stick look/anchoring, movement while aiming/shooting,
knife movement and reload movement.

Both exact native profiles and both local players have automated native-backed
unit/callback regressions. The release host run passed **142/142 cases**; Switch
builds, package validation and public audit passed. Host coverage does not extend
the physical acceptance to split-screen/player 2, the newer main, first-person,
every controller or every weapon/scene. Those combinations still need console
checks. No online/DLC/Mercenaries acceptance is claimed. Unusual scaled/parented
models and unsupported animation data fail closed.

Intermittent application startup and sleep/wake hangs were previously reported.
The user reported improvement after removing the old standalone debugging
overlays; the initiating cause and a universal fix remain unproven. Acceptance of
the RE5 camera/movement does not establish application lifecycle stability.

## Optional diagnostics

**Monitor** is explicitly opt-in, starts **Off** and resets to Off when the RE5
overlay reloads. It exposes authenticated process/build/module observations.
An empty metadata Build ID can be recovered only from a known bounded GNU note
plus the matching native identity published after startup validation. A note
alone cannot authorize writes; unknown, stale or changing processes remain blocked.

An explicit Monitor request may enable a two-second native observation lease.
Runtime counters and the retained camera observation from 0.28.26/0.28.27 help
inspect actual callback decisions, input, world transforms and camera stages.
The native observation adds no thread, server or automatic journal; collection
stays off until requested. It is separate from the retired RE4 / RE6 Telemetry
overlay and automatic frame journals, which are no longer packaged.

For application updates/report download, choose **Updates → Local update → Start
server** manually. This server starts Off and rejects RUN/automatic restart.
Reopen the app manually after transfer. Application reports retain the five
latest fixed files, at most 64 KiB each / 320 KiB total, within a 1 MiB budget.
They contain application stages rather than frame telemetry.
