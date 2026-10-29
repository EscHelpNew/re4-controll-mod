# Public OTA updates and release delivery

User authorizes preparing the existing GitHub repository for users and publishing
a release. Origin is https://github.com/EscHelpNew/re4-controll-mod.git. User will
make the repository public; do not change its visibility without instruction.

Add an Updates page to the EN/RU installer. It shows the repository and installed
version, checks the latest release, then lets the user download/install its NRO.
The fixed release assets are re4-controls.nro and re4-controls-update.txt.
The small manifest contains a strict version, byte count and SHA256. Fetch through
HTTPS with certificate/hostname verification; restrict redirects to HTTPS, limit
size/time, and handle private/offline/missing-release cases clearly.

Download in a worker while the main thread continues rendering and reload polling.
Verify exact size, SHA256, NRO structure and protocol2 bridge compatibility before
publication. Stage both bridge.nro and game.nro, preserve previous versions, and
restore them after a partial rename failure. Successful update takes effect when
the user reopens the application. No settings/token/game files are downloaded.
Explain that the updated game's bundled module is installed with Install/update mod.

Prepare a concise bilingual README, installation/troubleshooting guide, changelog,
reproducible build instructions, source/license notices, issue templates and CI.
Release only an allowlisted standalone NRO, public SD package, update manifest and
hashes; never include local project.json, keys, token, private logs or extracted game.
Keep experimental/hardware acceptance limits explicit. Publish only after the
new gyro/HOME/OTA code passes tests and build checks; test the real OTA path when
the repository/release is reachable from the console.
