"""Package only this project's allowlisted binaries; never game files or keys."""
from pathlib import Path
import hashlib
import json
import shutil
import zipfile
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.version import get_version
OUT = ROOT / 'build'


def package() -> None:
    config = json.loads((ROOT / 'project.json').read_text(encoding='utf-8-sig'))
    if config.get('sd_directory') != 're4-controls':
        raise ValueError('Review package allowlist for a different SD directory')
    nro = OUT / 're4-controls.nro'
    own = list((OUT / 'forwarder').glob('*.nsp'))
    if not own:
        from tools.launcher_bundle import export_nsp
        target=OUT/'forwarder/RE4-Controls-HOME.nsp'
        export_nsp((ROOT/'assets/home-launcher.bin').read_bytes(),target)
        own=[target]
    if len(own) != 1:
        raise RuntimeError('Build this project\'s own forwarder first')
    prefix = 'atmosphere/contents/010099A00BC1E000/exefs/'
    files = {
        prefix + 'subsdk9': OUT / 'experimental' / prefix / 'subsdk9',
        prefix + 'main.npdm': OUT / 'experimental' / prefix / 'main.npdm',
        'switch/.overlays/re4-controls.ovl': OUT / 'experimental/switch/.overlays/re4-controls.ovl',
        'atmosphere/contents/01002A000CD48000/exefs/subsdk9':OUT/'experimental/atmosphere/contents/01002A000CD48000/exefs/subsdk9',
        'atmosphere/contents/01002A000CD48000/exefs/main.npdm':OUT/'experimental/atmosphere/contents/01002A000CD48000/exefs/main.npdm',
        'switch/.overlays/re6-telemetry.ovl':OUT/'experimental/switch/.overlays/re6-telemetry.ovl',
        f'switch/{config["sd_directory"]}/bridge.nro': nro,
        f'switch/{config["sd_directory"]}/game.nro': nro,
        'install/re4-controls-forwarder.nsp': own[0],
        'README-install.md': ROOT / 'docs/installation.md',
        'README-measurement-ru.md':ROOT/'docs/measurement-ru.md',
        'licenses/MIT.txt': ROOT / 'LICENSE',
        'licenses/exlaunch-GPL2.txt': ROOT / 'third_party/exlaunch/LICENSE',
        'licenses/libtesla-GPL2.txt': ROOT / 'third_party/libtesla/LICENSE',
        'licenses/Switch-Debug-Kit-MIT.txt': ROOT / 'third_party/switch-debug-kit/LICENSE',
        'licenses/nx-ovlloader-GPL2.txt': ROOT / 'third_party/nx-ovlloader/LICENSE.md',
        'licenses/graphics-NOTICES.txt': ROOT / 'third_party/graphics-NOTICES.txt',
        'licenses/NTON-GPL3.txt': ROOT / 'third_party/NTON-GPL3.txt',
    }
    manifest = {'status': 'experimental; core controls and updated gyro/HOME confirmed on hardware',
                'version': get_version(),
                'features': ['proportional turning and walking', 'camera/aim sensitivity',
                             'additive firearm-aim gyro', 'English/Russian settings',
                             'four stick response curves', 'themed reloadable installer',
                             'separate right-stick aiming','10ms gyro average and angular deadzone','public GitHub OTA updates',
                             '20-button mapping including menus/QTE','adjustable camera return and free 360 orbit',
                             'native-default graphics: filtering, handheld SSAA, object culling and shadows',
                             'independent controls/graphics Default and Custom profiles',
                             'separate measured-reference RE6 controls preset with faster overview and slower aiming',
                             'optional first-person camera and projected dot/crosshair',
                             'optional native-budget Leon movement while aiming, using knife or ordinary reload',
                             'native Default, measured RE6 and Custom controls in the first inline profile row',
                             'frame-clock gyro interpolation and guarded native raise pitch continuity',
                             'grouped settings and root-only language selection',
                             'live reticle shape and size picture preview',
                             'read-only guarded RE6 observer and synchronized RE4/RE6 journals',
                             'explicit Start/Stop recording, hidden overlay SD writer and verified CSV download'],
                'verified': ['user confirmed movement/camera/aim in 0.2.0',
                             'RE6 console capture:1437 valid synchronized samples without loss in0.11.2',
                             'user accepted RE6 preset in RE4 in0.12.0;1495-sample baseline without loss',
                             'user confirmed additive aim-only gyro, vertical range, help and speed caps in 0.2.2',
                             'overlay UI on existing Uberhand',
                             'user confirmed themed installer and mod installation in 0.3.0',
                             'user confirmed EN/RU, four curves and removed walk-speed menu in 0.3.0',
                             'user confirmed gyro filter/speed/deadzone and automatic HOME icon in 0.3.1',
                             'user confirmed right-stick aim zero/variable-speed behavior in 0.4.0',
                             'user confirmed restored game startup after hook-pool fix',
                             'diagnostic sticks/gyro on console', 'NRO changed-image reload and new PID', 'cold-launch credential'],
                'pending': ['0.13 gyro/raise polish and ordinary reload movement hardware acceptance',
                            'further controlled RE6/RE4 measurements',
                            'anonymous OTA installation on console',
                            'graphics visual/performance hardware acceptance',
                            'independent profile switching hardware acceptance',
                            'first-person placement, visibility and aim transitions on hardware',
                            'projected reticle and native gait/collision/attack hardware acceptance',
                            'free camera and button mapping hardware acceptance',
                            'special weapons and all gameplay contexts', 'controller switch in game'],
                'files': {name: hashlib.sha256(path.read_bytes()).hexdigest() for name, path in files.items()}}
    destination = OUT / 're4-controls-experimental.zip'
    local = OUT / 'local-sd'
    # Rebuild this generated tree so removed optional artifacts cannot linger
    # and overwrite an existing console's overlay menu on a later copy.
    if local.exists():
        if local.resolve().parent != OUT.resolve() or local.name != 'local-sd':
            raise RuntimeError('Unexpected generated SD tree path')
        shutil.rmtree(local)
    with zipfile.ZipFile(destination, 'w', zipfile.ZIP_DEFLATED) as archive:
        for name, path in files.items():
            archive.write(path, name)
            target = local / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, target)
        archive.writestr('manifest.json', json.dumps(manifest, indent=2) + '\n')
    token = local / f'switch/{config["sd_directory"]}/token.txt'
    if (ROOT/'data/token.txt').is_file():shutil.copy2(ROOT / 'data/token.txt', token)
    (OUT / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print('Public package:', destination)
    print('Local SD tree (credential copied only when configured):', local)


if __name__ == '__main__':
    package()
