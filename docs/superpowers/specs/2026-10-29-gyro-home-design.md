# Gyro sampling and automatic own HOME launcher

User requests a working firearm-only gyro, adjustable motion speed and deadzone,
the mean of two readings separated by10ms, and creation of the app's HOME icon
at application startup. Existing EN/RU, stick curves and native speed caps stay.
User opted to proceed directly and requests diagnosis on the new build.

Gyro: subtract calibration bias, average two angular-rate measurements separated
by10ms of sensor time, apply a soft per-axis deadzone in degrees/second, then
multiply by master speed, axis gain, direction and elapsed seconds. No sleep in
the game thread; consume the SDK's historical sensor samples oldest-first.
Do not carry a pair across inactive aim, disconnect or pause. Add explicit fresh
sample/aim/filter/recovery diagnostics. Investigate the existing failure with
real reports; bounded sensor recovery must not introduce an IPC retry storm.

Settings wirev5 keeps64bytes: the obsolete calibration-request word stores the
gyro deadzone float, the removed walk-gain word stores master gyro speed.
Versions1–4 migrate without losing gyro enable, gains, language or curves.
SharedABI8 prevents older adapters from ignoring the new interpretation.

HOME: bundle only this app's three generated NCAs and validated metadata, using
the existing same ProgramId0100E4C04E540000 and bridge path. Installation uses
NCM placeholders, commits metadata only after all content exists, pushes the own
application record and invalidates only this app's control cache. No game tickets,
game files or keys are shipped. Idempotent startup skips current content; preserve
old NCAs and metadata for rollback. Unsupported/applet launches report a useful
status instead of blocking the application. Do not delete the running title.

Implementation choice: prebuilt own forwarder payload, rather than generating
encrypted NCAs on the console. This avoids storing console keys in the NRO.
Diagnostic/game/overlay tests must cover real adapters with platform calls stubbed.
