# RE4 reference-mode correction

Hardware0.28.15 restored digits but the same real Handgun/Professional enemy stillshows10. The exact live rejected predicate is notyet observed. Native re-audit proves one concrete error:1CCCC0 branches on byte84F0<=1 versus>1, not modes5/6; enemy1D1520 uses rank byte500C without an84F0read. Canonical arithmetic mustaccept byte modes2..255 and validranks0..10, includingmode6/ranksotherthan10. Preserve0/1 legacyfallback untiltheir random/easy semantics aredefined.

Do notchange fonts, geometry, fade, labels, HPbar ratio oractual health/damage. No new diagnostic UI. Extend only the EXISTING opt-in diagnostic SNAPSHOT JSON with a boundedread-only health observation usingexistingconnectedGameLink, so actualmode/rank/coefficient/class rejection canbe measuredifstillneeded. DefaultNormal/Off andservice lifecycle unchanged. NoHPnativeworker orthreadsneededforpurearithmeticfix; standaloneREControlshang occursoutsidegamecalc.

Separately strengthen existing5x64KiBAppJournal stage persistence withcheckedfsync/_commit afterfflush. Thisimprovesdataevidence butisnotclaimed asfreeze fix orpower-lossguarantee. Nativeguards/proofs, TDD/callback/IPC/formatter tests, fullsuite+allbuilds/audit, version0.28.16 andphysicalacceptance required.
