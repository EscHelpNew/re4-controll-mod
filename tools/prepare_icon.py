"""Encode the user's unchanged artwork in the Switch icon asset format."""
from pathlib import Path


def prepare(source: Path, destination: Path) -> Path:
    from PIL import Image, ImageOps
    if source.resolve() == destination.resolve():
        raise ValueError('The original icon must not be overwritten')
    with Image.open(source) as image:
        # Format conversion for the app container: no cropping or added text.
        image = ImageOps.pad(image.convert('RGB'), (256, 256),
                             method=Image.Resampling.LANCZOS, color='black')
        destination.parent.mkdir(parents=True, exist_ok=True)
        image.save(destination, format='JPEG', quality=95, optimize=True)
    if destination.stat().st_size > 0x20000:
        raise ValueError('Switch icon exceeds 128 KiB')
    return destination
