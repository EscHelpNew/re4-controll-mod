"""Use this repository's pinned Switch Debug Kit configuration and credentials."""
from pathlib import Path
import runpy
import sys

KIT = Path(__file__).resolve().parents[1] / 'third_party/switch-debug-kit'
ROOT = KIT.parents[1]
sys.path.insert(0, str(KIT / 'host'))
import project
original_load = project.load
project.load = lambda root=ROOT: original_load(ROOT)
if not any(arg == '--token-file' or arg.startswith('--token-file=') for arg in sys.argv[1:]):
    sys.argv[1:1] = ['--token-file', str(ROOT / 'data/token.txt')]
runpy.run_path(str(KIT / 'host/reload.py'), run_name='__main__')
