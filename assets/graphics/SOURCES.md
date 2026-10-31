# Graphics menu screenshot sources

These are genuine gameplay screenshots from the official Nintendo US galleries
for the 2019 Nintendo Switch ports. RE4 is the original 2005 game, not the 2023
remake. Retrieved on 2026-10-31. The downloaded JPEG bytes are unchanged.

Game imagery copyright: CAPCOM CO., LTD. All rights reserved. Nintendo hosts
these official promotional screenshots; the repository's software license does
not apply to this imagery. Attribution records its provenance and does not
imply that Nintendo or Capcom endorses the mod.

## RE4

- File: `re4-source.jpg`, JPEG RGB, 1280 x 720.
- [Nintendo source page](https://www.nintendo.com/us/store/products/resident-evil-4-switch/), product `70010000012858`, Switch release 2019-05-21.
- [Direct original JPEG](https://assets.nintendo.com/image/upload/store/software/switch/70010000012858/3d15e1e1ba3eab561172e2ec7c2e5fc31803654ff417d842145e110a98bdc41e.jpg).
- SHA-256: `3d15e1e1ba3eab561172e2ec7c2e5fc31803654ff417d842145e110a98bdc41e`.
- Gallery image 3 (excluding video): Leon aiming in the village. Ground,
  fence edges, shadows and distant buildings provide regions for illustration.

## RE6

- File: `re6-source.jpg`, JPEG RGB, 1280 x 720.
- [Nintendo source page](https://www.nintendo.com/us/store/products/resident-evil-6-switch/), product `70010000015880`, Switch release 2019-10-29.
- [Direct original JPEG](https://assets.nintendo.com/image/upload/store/software/switch/70010000015880/e8a61f6b586aaa05eb0bb95daab16adbaf861a9d752ac29d17b4c12e31bad1dc.jpg).
- SHA-256: `e8a61f6b586aaa05eb0bb95daab16adbaf861a9d752ac29d17b4c12e31bad1dc`.
- Gallery image 4 (excluding video): Leon aiming at a creature in a street.
  Ground and building outlines provide regions for texture/edge illustration.

`sources.json` records the same provenance in a machine-readable form. The
gallery assets and release dates were read from each public product page's
`__NEXT_DATA__` product record. No account or private capture was used.

`tools/prepare_graphics.py` checks the source identities, hashes and dimensions,
then fits the complete image within 320 x 180 without cropping and quantizes it
to RGB565. Only resizing, black padding when needed and format conversion are
performed. Menu region outlines are separate UI elements. These pictures show
illustrative scenes, not before/after results or captures of a selected mod
setting. Generated arrays are embedded in the build; no network fetch occurs
at runtime.
