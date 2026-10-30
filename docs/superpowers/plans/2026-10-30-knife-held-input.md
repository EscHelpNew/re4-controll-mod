# Knife movement input regression

The user confirmed Move while using knife is enabled, but movement is absent
both in stance and strike. Gun aiming movement works.

Native338330 reads the pad block at124EC38, then byte+19 bit3. The held word
begins at124EC50 (+18), so ordinary controller knife hold is0x800 (bit11).
The mod used bit48, which belongs to another input region. Existing fixtures
repeated that incorrect assumption and therefore passed without real input.

- [x] Change fixtures to the independently derived literal0x800 and observe
  failing gate and actual-module aiming-routing tests before changing the adapter.
- [x] Share the verified knife hold mask between movement qualification and
  analog knife aiming. Preserve firearm0x10 and firearm-only gyro behavior.
- [x] Test stance and active strike translation/gait before hit and collision
  processing; reject old bit48, firearm-only input, release and option-off.
- [ ] Run complete tests and Switch build, independent review, credential audit,
  main/CI and library-only console delivery. Keep GitHub release hold active.

No state-table widening or new hooks are required. Native hit timing, gait
budget, pose restoration and collision ordering retain their existing paths.
Hardware acceptance is required after installing0.7.3.
