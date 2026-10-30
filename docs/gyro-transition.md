# Gyro entry and continuous sensor output

The0.7.2 adapter waits for native aim readiness instead of a fixed weapon-raise
timeout. The build-specific handler slot is main+1265A28. Eight examined ordinary
weapon handlers select actor state0/6/1/1 after native raising completion:
31CC60,31B9F0,3212E0,3270F0,332CC0,333DF0,3351A0,342F90.

The gate arms only when sampling sees that state, discards that entire FIFO batch,
and preserves the session through firing and recovery. In particular1/4 means
both initial raising and post-shot recovery; testing it without session history
would either admit the original snap or interrupt gyro after every shot. Native
shot state2 remains valid once armed. Reload, release, actor/weapon identity,
controller discontinuity and frozen/paused contexts invalidate the session.
Consumption rechecks state and identity, but cannot arm from a transition that
happened after sampling. Routine15 has a separate native dispatcher and retains
its prior behavior; ordinary readiness is not assumed for mounted interactions.

The input filter retains endpoints10ms apart. It interpolates the older endpoint
when intervals vary and integrates their mean for every new sensor interval.
This removes the old zero/10ms output alternation while preserving the requested
two-reading formula, sensitivity and soft deadzone. Initial anchoring, duplicate
samples, malformed samples and long gaps produce no replay.

Native33DF80 copies current pitch through33EC50 and then overwrites target pitch
at33E608. Therefore writing only the target cannot supply universal native easing:
some branches discard that delta entirely. Updates to both pitch fields and the
manual-input sway flag remain. Native local-yaw bounds vary by control mode;
this change does not substitute an unverified universal bound.

Hardware acceptance: hold aim and immediately tilt downward during raising; keep
the controller still when ready. There should be no delayed snap. Then sweep
slowly, fire repeatedly, reload, release/re-aim, switch weapons and visit HOME.
Compare gyro with the right stick using the same weapons and aim endpoints.
Host tests verify input/state behavior; perceived animation smoothness requires
this console check. This does not claim RE9 animation or camera equivalence.
