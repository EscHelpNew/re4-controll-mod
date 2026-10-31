# Telemetry overlay

This Tesla overlay records the native read-only journal for either the supported
RE6 build or the RE4 profile in `include/re4/profile.hpp`. Select the game, choose
**Start recording**, hide the overlay with B or the Tesla combination, play, then
reopen it and choose **Stop and save**. **Exit (save recording)** also drains and
saves an active capture before unloading. The language setting belongs to this
overlay and is stored in `/config/re4-telemetry-language.txt`.

The output directory is `sdmc:/switch/re4-controls/telemetry/`. During recording,
the file ends in `.csv.part`; only successful flush, filesystem commit, close,
and rename produce `.csv`. Failed captures retain `.part`. The private `r6log:`
mount points to the same SD directory and remains mounted until after the worker
has joined. The overlay displays its filename, records, missed records, errors,
and last IPC result.

Start attaches at the producer tail. Earlier frames are neither recorded nor
counted as lost. `#capture` records this starting sequence and ring capacity.
Stop reads one stable published-sequence snapshot and drains only through that
sequence, even while the game keeps producing. Overwritten records are counted;
the final `#end` line includes total records and drops. Readers should ignore
lines beginning with `#` when parsing the CSV table. The journal metadata includes
the full title/build identity, native tick frequency, and producer session.

The raw CSV comes from `include/re6/journal.hpp`: native frame/tick, input sample
identity/tick/buttons/sticks, and player/view/projection matrices. Validity bits
identify missing or non-finite groups, whose CSV cells remain empty. This
collector does not estimate positions, orientation conventions, frame rate, or
world units. Both games use the same CSV schema.

The worker owns `dmnt:cht` and uses commands 65003 (attach), 65002 (metadata),
65101 (mappings), and 65102 (read). It scans every readable mutable-code mapping,
including BSS beyond the first 128 KiB, up to 512 MiB and 8192 mapping entries in
total. Header reads overlap pages and validate the entire ring extent. A 64 KiB
worker stack, 16 KiB stdio buffer, and 4 KiB CSV buffer bound collector memory.
The Tesla loop explicitly uses `LaunchFlags::None`: the default
`CloseOnExit` would unload the overlay on B and stop recording.

Build with the installed devkitA64/libnx toolchain and the vendored
`third_party/libtesla/include`, following `build.py`'s overlay flags. Compile and
link only `source/re6_overlay/main.cpp` and `source/re6_overlay/worker.cpp`, with
`-Iinclude -Isource/re6_overlay`, C++20, the libnx `switch.specs`, `-lnx -lm`, then
package the ELF with `nacptool` and `elf2nro` as an `.ovl`. No settings sources or
network libraries are required. The collector's portable tests run with:

```powershell
C:/msys64/mingw64/bin/g++.exe -std=c++20 -Wall -Wextra -Werror -static -Iinclude tests/re6_recorder_tests.cpp -o build/re6_recorder_tests.exe
./build/re6_recorder_tests.exe
```

Host tests and an actual devkitA64 compile/link verify the collector logic and
Tesla integration. Console validation is still required for dmnt permissions,
game-specific hook coverage, SD behavior, and sustained recording performance.

`telemetry-status.csv` is a separate diagnostic schema, not a game journal. It
retains the target/actual build, operation phase, IPC result and recorder error,
including connection failures before a sample file exists. It is updated on
state/error transitions instead of every frame. On Switch, filename replacement
uses a complete `.previous` recovery copy because Horizon cannot replace an
existing file with one atomic rename. A diagnostic write failure is shown
separately and never changes the recorder's failure reason.

Discovery checks committed slot sequences as well as header metadata. The
producer's private state contains another valid-looking Header before public
storage; accepting that copy produced zero-record incomplete captures in 0.11.0.
The updated overlay can discover the already-installed native module correctly.
