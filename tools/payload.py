"""Rebuild the generated RomFS from a fixed allowlist."""
from pathlib import Path
import shutil
FILES={'app-logo.bmp','app-logo-re6.bmp','home-launcher.bin','subsdk9','main.npdm','re4-controls.ovl'}
RE6_FILES={'re6-subsdk9','re6-main.npdm','re6-telemetry.ovl'}
def stage_payload(root,files):
    root=Path(root).resolve();base=root/'build';stage=base/'diagnostic-romfs'
    if set(files) not in (FILES,FILES|RE6_FILES):raise ValueError('Unexpected or incomplete NRO payload files')
    if not all(Path(p).is_file() for p in files.values()):raise ValueError('Build the complete module/overlay before NRO')
    resolved=stage.resolve()
    resolved.relative_to(root)
    if resolved.parent!=base.resolve() or resolved.name!='diagnostic-romfs':raise ValueError('Unexpected generated RomFS path')
    if stage.exists():shutil.rmtree(stage)
    stage.mkdir(parents=True)
    for name,source in files.items():shutil.copy2(source,stage/name)
    return stage
