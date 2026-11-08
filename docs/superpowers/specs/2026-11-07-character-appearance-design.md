# Cosmetic player appearance

The requested default is Leon. Other choices are Ashley, Ada, Luis Sera,
Krauser, Merchant, human Ramon Salazar and an intact village Ganado. Selecting
Ashley must make the controlled Leon visibly Ashley while retaining Leon's
weapons, inventory, actions, collision and story state. The overlay should apply
a choice during play when the native resource lifecycle permits it.

## Approach

The native character setter changes gameplay and is unsuitable. Copying a donor
rig over Leon also breaks animation and native attachments. A render proxy would
introduce a second actor pointer into queued shadows and drawing.

The current implementation direction is to keep the original player and rig,
substitute an owned visual chain as drawing/skinning function arguments, and
adapt donor weight matrices to Leon's major joints. This is an engineering
choice derived from the native renderer. Secondary donor bones follow their
nearest major joint; facial and cloth simulation are outside this first adapter.
Retain native weapon hands only after proving their chain/visibility ownership.

## Resource and UI contract

Read donor resources from the installed game. Do not redistribute meshes,
textures or game binaries. Validate archive boundaries, bone hierarchy, weights,
finite coordinates and piece recipes before relocation or rendering. A failed
load retains native Leon and reports failure instead of claiming an active skin.

UI writes desired appearance; game task loads resources; renderer only consumes
a completed immutable generation. Actual player type and costume bytes remain
unchanged. Release old resources only through a proven native completion and
heap/pool lifecycle. Room transitions and concurrent weapon changes invalidate
unready requests. Never use a fixed elapsed-frame count as a GPU completion test.

The standalone app and overlay will use the same Game appearance submenu.
Persisted settings require a new wire/ABI version after deployed 0.22.0.
Desired, pending and applied states must be distinguishable. Unimplemented
donors must not appear as working choices.

## Verification gates

First build pure archive/retarget kernels with synthetic malformed-input tests.
Validate against privately installed assets without copying them into public
fixtures. Then prove raw-load and resource retirement on the console before
enabling drawing. Finally exercise each supported donor while moving, aiming,
reloading, switching weapons, changing rooms and restoring Leon. Host math tests
do not establish visual or resource-lifetime correctness in the game.

Status: local implementation, host verification and ARM64 builds are complete.
The first console transfer and physical gameplay/visual checks are pending.

## Final implementation decisions under verification

- Use direct native position/normal evaluators with owned deformation buffers;
  donor skinning must not consume the shared native frame arena or starve actors.
- Per-slot ordered CPU acknowledgements admit source-byte reuse because native
  mesh commands copy those arrays into their own GPU resources. Bank retirement
  and initial GPU uploads retain explicit GPU completion acknowledgements.
- The loader is asynchronous internally. Keep its pack pin through actual IO
  completion; a GPU fence is not a file-completion signal. The blocked producer
  retains the exclusive cooperative scheduler baton (separate OS threads).
- Reserve the actual native pack allocation before entering the loader, hand
  over its validated file handle and allocation to native ownership, and keep
  at least128 MiB largest contiguous heap headroom. Reject the alternate YZ2
  copy path. All20 native IO slots must be idle at admission; transient busy
  state retries without pretending the selection has applied.
- Preserve a bounded passive cache of the seven donor packs. Suppressed old
  unload requests are not replayed after a different native consumer may adopt
  the pack. Future native group unloads reclaim unpinned packs. The worst-case
  seven-pack CPU footprint is about196 MiB; this is a deliberate bounded cache
  choice, not a refcount acquisition or proof of every future engine allocation.
