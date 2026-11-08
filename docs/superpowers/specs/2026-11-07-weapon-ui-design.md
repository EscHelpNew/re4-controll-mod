# Weapon feedback and pickup polish

The requested preview was supplied before implementation. The weapon popup uses
the existing Leon inscription as its type reference: gray bold italic serif,
comparable capital height, no outline, immediately left of the health HUD.
A light-gray translucent mist sits behind the text, extending generously left,
right and down with irregular feathered edges, following the user's annotated
preview. It softly gathers on appearance and disperses on disappearance.
Text and mist share opacity and timing.

The popup is part of the existing D-pad weapon switching option, with no separate
toggle. Only a successful D-pad equip triggers it, including grenade cycling.
Fade in takes 500 ms, full display 2000 ms, and fade out 500 ms. A new successful
switch replaces the previous label immediately and restarts this timeline.
Disabling the option or leaving gameplay clears the popup; failed equips and
inventory selection do not trigger it. HUD visibility follows the native HUD.

In the attache case, hovering a firearm shows its current upgraded firepower
under its name, formatted to one decimal place (preview: `Урон: 1.0`). Read the
selected item instance and native upgrade calculation. Do not show power for
grenades, ammunition or other items. This contextual readout is independent of
the D-pad setting. Console verification of layout and values remains necessary.

Skipping pickup confirmation must preserve successful native acquisition and
inventory-full handling while preventing the confirmation's whole-frame flash.
Successfully taking a firearm must play its original short pickup sound once.
Rejected pickups must not play a success sound or modify inventory.

Retain all existing 0.23 functionality and unsupported-build guards. Do not ship
original game assets. Host fixtures and builds are not console verification.
