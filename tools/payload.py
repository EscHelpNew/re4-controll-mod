"""Rebuild the generated RomFS from a fixed allowlist."""
from pathlib import Path
import shutil
FILES={'app-logo.bmp','home-launcher.bin','subsdk9','main.npdm','re4-controls.ovl'}
def stage_payload(root,files):
    root=Path(root).resolve();base=root/'build';stage=base/'diagnostic-romfs'
    if set(files)!=FILES:raise ValueError('Unexpected NRO payload files')
    if not all(Path(p).is_file() for p in files.values()):raise ValueError('Build the complete module/overlay before NRO')
    resolved=stage.resolve()
    resolved.relative_to(root)
    if resolved.parent!=base.resolve() or resolved.name!='diagnostic-romfs':raise ValueError('Unexpected generated RomFS path')
    if stage.exists():shutil.rmtree(stage)
    stage.mkdir(parents=True)
    for name,source in files.items():shutil.copy2(source,stage/name)
    return stage
