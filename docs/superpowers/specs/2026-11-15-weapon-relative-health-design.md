# Weapon-relative health numbers and boss health owners

The user chose a normal body-shot reference before the first hit. Headshots,
criticals and other bonuses remove more displayed HP naturally. Neither native
Firepower upgrades nor the optional extra upgrade may increase the enemy's
displayed HP or change its scale. Bosses use the existing small world-space bar
while aimed, rather than a new encounter HUD.

## Chosen approach

Use a pure, native-qualified prediction of damage at a fixed synthetic Firepower
of1.0 for the current weapon class and enemy's ordinary body-damage context.
Display raw currentHP / referenceRawDamage, rounded once to one decimal. Keep
bar length and colour based on authoritative raw current/maxHP. The reference
never uses the equipped Firepower nibble, extended ownership, ammo count or the
last actual hit. Actual head/critical/upgrade damage only reduces rawHP.

Example: reference58rawHP gives1500/58 =25.9 displayedHP for both Firepower2.0
and2.2. Their observed native losses116/128rawHP leave23.9/23.7, respectively.
Integer damage and display rounding remain native.

Alternatives considered: dividing by the current shot's power/damage ratio can
make HP change slightly after upgrades; calibrating from the last hit changes
scale on crits and gives no number before the first shot. Both conflict with
explicit user choices. Calling attack functions for prediction can mutate game
state or consume randomness and is excluded.

## Runtime boundaries

Only cloned arithmetic qualified against this exact supported Switch image may
supply a positive reference. Unsupported, immune, zero and special damage paths
keep their actual raw bar but omit the number rather than inventing a scale.
Native tables and target fields are read during gameplay capture. Render code
receives scalars only through the existing sequence-protected snapshots.

A stable weapon/class stamp excludes Firepower upgrades, extra toggles and
rounds. Changing weapon class invalidates old faded numbers; raw bars retain
their normal fade. Settings/wire/save formats stay unchanged. No new telemetry,
worker, physics query or enemy/damage write is introduced.

## Boss ownership

Resolve the native authoritative HP owner for the aimed boss part using exact
active-pool, type, subtype, vtable and reciprocal-link validation. Preserve the
aimed part's world anchor and copy HP scalars from the owner. Never sum arbitrary
actors or mix a body currentHP with a different core's maximumHP. A phase reset
shows that phase's native current/max pair.

Qualified corrections target Saddler31 body0 to reciprocal core1 and Salazar38
parts to reciprocal core3. Other validated native actors retain their own HP.
Del Lago's harpoon/custom camera path needs independent qualification and must
not be enabled by weakening the ordinary firearm-scene gates.
