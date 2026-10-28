"""Build pinned upstream nx-ovlloader with the local devkitPro/libnx."""
from pathlib import Path
import json
import os
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build'
stage = OUT / 'overlay-loader'
shutil.copytree(ROOT / 'third_party/nx-ovlloader', stage, dirs_exist_ok=True)
source = stage / 'source/main.c'
# libnx 4.12 removed the old NORETURN compatibility macro.
source.write_text(source.read_text().replace('NORETURN', '__attribute__((noreturn))'))
config = json.loads((stage / 'ovll.json').read_text())
for source, target in [('title_id', 'program_id'), ('title_id_range_min', 'program_id_range_min'),
                       ('title_id_range_max', 'program_id_range_max')]:
    config[target] = config[source]
config.update(program_index=0, allow_system_resource_pool=True,
              enable_alias_region_extra_size=False, prevent_code_reads=False,
              signature_key_generation=0, system_resource_size='0x0', version='0x00000000',
              process_category=0, optimize_memory_allocation=False,
              disable_device_address_space_merge=False)
(stage / 'ovll.json').write_text(json.dumps(config, indent=2))
env = os.environ.copy()
env['DEVKITPRO'] = '/opt/devkitpro'
env['DEVKITA64'] = '/opt/devkitpro/devkitA64'
temporary = (OUT / 'tmp').resolve()
temporary.mkdir(exist_ok=True)
env['TMPDIR'] = '/' + temporary.drive[0].lower() + temporary.as_posix()[2:]
env['PATH'] = os.pathsep.join(['C:/msys64/usr/bin', 'C:/msys64/opt/devkitpro/devkitA64/bin',
                              'C:/msys64/opt/devkitpro/tools/bin', env.get('PATH', '')])
subprocess.run(['C:/msys64/usr/bin/make.exe', '-j4', 'RELEASE=1'], cwd=stage, env=env, check=True)
destination = OUT / 'diagnostic-romfs/nx-ovlloader.nsp'
destination.parent.mkdir(exist_ok=True)
shutil.copy2(stage / 'ovll.nsp', destination)
(destination.parent / 'boot2.flag').write_bytes(b'')
print('Built pinned upstream overlay loader:', destination)
