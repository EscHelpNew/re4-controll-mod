# Downloadable system crash evidence for application sleep hangs

The user reports0.28.29 cannot wake even after Local Update fully stops. Five own app journals were downloaded successfully, but they contain neither a fault stack nor sleep/resume boundaries. The cause is not established. Off requires worker join and socket cleanup, so an active listener is not necessary for this reproduction; prior network initialization remains a separate possibility.

Extend only the already authorized authenticated CRASH download: retain the five64KiB own app journals and append a read-only snapshot of up to five newest files across the three fixed Atmosphere crash/fatal directories. Text contents are bounded64KiB per file; binary files have metadata only. Enumerations and total output are bounded below1MiB. No arbitrary SD path access, new stored reports, background networking, per-frame disk writes, sleep/focus overrides or new workers. Missing evidence must be reported without blocking normal startup. The snapshot runs only for an explicit report request while the user has enabled Local Update.

This is evidence collection, not a sleep fix. Neither absent system reports nor missing own clean_exit proves a particular cause. Keep the known sleep/startup problem in release notes. Version0.28.30 is reserved for any delivered changed candidate. RE5 game code and pending0.28.29 gameplay acceptance are preserved.

## Reviewed boundaries

Production passes the fixed local mount sdmc:. Host callers must trust the ancestors of their supplied root; the collector rejects reparse/symlink root, atmosphere and final report directories but does not traverse every root ancestor. Enumeration is capped at2048 entries per directory with a visible scan-limit marker. Byte/entry bounds are not a time deadline for synchronous filesystem operations. A concurrent external directory replacement is not an atomic snapshot.
