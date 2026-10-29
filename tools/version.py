"""Read the public application version from its single C++ source."""
from pathlib import Path
import re
ROOT=Path(__file__).resolve().parents[1]
def get_version():
    match=re.search(r'kVersion="([0-9]+\.[0-9]+\.[0-9]+)"',(ROOT/'include/re4/version.hpp').read_text())
    if not match:raise ValueError('Invalid application version')
    return match[1]
