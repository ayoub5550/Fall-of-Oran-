"""Prepare raw textures for UE import: copies PBR sets from the Godot repo (ambientCG CC0),
rotates road textures so lanes run along +X under world-aligned projection, and generates
shop signs (Arabic/French, Oran shop names), window glass tiles and the PORT sign with PIL.
Output: RawAssets/Textures/*.png|jpg  → imported by import_assets.py
"""
import os, shutil, random
from PIL import Image, ImageDraw, ImageFont, ImageFilter
import numpy as np

SRC = "/work/repos/fall-of-oran/assets"
OUT = "/work/repos/fall-of-oran-ue5/RawAssets/Textures"
os.makedirs(OUT + "/Signs", exist_ok=True)
KUFI = "/usr/share/fonts/truetype/noto/NotoKufiArabic-Regular.ttf"
NASKH = "/usr/share/fonts/truetype/noto/NotoNaskhArabic-Bold.ttf"
LATIN = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"


def arabic(text):
    # shape + reorder for PIL (which does no bidi/shaping)
    import arabic_reshaper
    from bidi.algorithm import get_display
    return get_display(arabic_reshaper.reshape(text))


def copy_pbr():
    fams = ["asphalt", "bricks", "concrete", "ground", "metal", "paving", "plaster", "road"]
    for f in fams:
        for k in ("color", "normal", "rough"):
            src = f"{SRC}/tex/{f}_{k}.jpg"
            im = Image.open(src)
            if f == "road":
                im = im.rotate(90, expand=True)
            if im.width > 2048:
                im = im.resize((2048, 2048), Image.LANCZOS)
            im.save(f"{OUT}/T_{f}_{k}.jpg", quality=92)
    shutil.copy(f"{SRC}/tex/sky_pano.jpg", f"{OUT}/T_sky_pano.jpg")
    for n, o in (("blood_decal.png", "T_blood.png"), ("puddle.png", "T_puddle.png"), ("palm_frond.png", "T_palm_frond.png")):
        shutil.copy(f"{SRC}/{n}", f"{OUT}/{o}")
    print("pbr copied")


def windows():
    # lit window: warm glow, curtain folds, dark frame cross
    w, h = 256, 512
    im = Image.new("RGB", (w, h), (235, 190, 120))
    d = ImageDraw.Draw(im)
    for x in range(0, w, 18):
        c = 200 + int(30 * np.sin(x * 0.4))
        d.rectangle([x, 0, x + 9, h], fill=(c, c - 50, c - 110))
    d.rectangle([0, 0, w, h], outline=(30, 25, 20), width=14)
    d.rectangle([w // 2 - 6, 0, w // 2 + 6, h], fill=(30, 25, 20))
    d.rectangle([0, h // 2 - 6, w, h // 2 + 6], fill=(30, 25, 20))
    im = im.filter(ImageFilter.GaussianBlur(1.2))
    im.save(f"{OUT}/T_window_lit.png")
    # dark window: glass with faint sky reflection gradient
    im = Image.new("RGB", (w, h))
    px = np.zeros((h, w, 3), np.uint8)
    for y in range(h):
        t = y / h
        px[y, :] = (int(28 + 30 * (1 - t)), int(34 + 36 * (1 - t)), int(52 + 46 * (1 - t)))
    im = Image.fromarray(px)
    d = ImageDraw.Draw(im)
    d.rectangle([0, 0, w, h], outline=(22, 20, 18), width=14)
    d.rectangle([w // 2 - 6, 0, w // 2 + 6, h], fill=(22, 20, 18))
    d.rectangle([0, h // 2 - 6, w, h // 2 + 6], fill=(22, 20, 18))
    im.save(f"{OUT}/T_window_dark.png")
    print("windows done")


SHOPS = [
    ("مكتبة الأمل", "LIBRAIRIE", (210, 190, 150), (40, 30, 25)),
    ("صيدلية وهران", "PHARMACIE", (30, 120, 70), (240, 240, 230)),
    ("مقهى الباهية", "CAFÉ EL BAHIA", (120, 30, 25), (240, 220, 180)),
    ("مخبزة السلام", "BOULANGERIE", (200, 160, 90), (50, 30, 15)),
    ("حلاق عصري", "COIFFEUR", (30, 60, 120), (230, 230, 240)),
    ("مطعم سيدي الهواري", "RESTAURANT", (150, 50, 20), (250, 230, 190)),
    ("ملابس نسائية", "BOUTIQUE", (60, 30, 80), (240, 220, 240)),
    ("هاتف نقال — TAXIPHONE", "TÉLÉPHONE · INTERNET", (20, 90, 140), (255, 240, 200)),
    ("بقالة الأخوة", "ALIMENTATION GÉNÉRALE", (40, 110, 60), (245, 235, 200)),
    ("إلكترونيات النجمة", "ÉLECTRONIQUE", (30, 30, 35), (255, 200, 60)),
    ("جزارة الحلال", "BOUCHERIE", (170, 30, 30), (255, 245, 235)),
    ("محل حلويات", "PÂTISSERIE", (230, 120, 150), (60, 20, 40)),
]


def signs():
    w, h = 1024, 128
    for i, (ar, fr, bg, fg) in enumerate(SHOPS):
        im = Image.new("RGB", (w, h), bg)
        d = ImageDraw.Draw(im)
        # grime + scratches
        px = np.array(im).astype(np.int16)
        noise = np.random.default_rng(i).normal(0, 9, (h, w, 1))
        px = np.clip(px + noise, 0, 255).astype(np.uint8)
        im = Image.fromarray(px)
        d = ImageDraw.Draw(im)
        fa = ImageFont.truetype(KUFI, 58)
        ff = ImageFont.truetype(LATIN, 30)
        ta = arabic(ar)
        bb = d.textbbox((0, 0), ta, font=fa)
        d.text(((w - (bb[2] - bb[0])) / 2 - bb[0], 6 - bb[1]), ta, font=fa, fill=fg)
        bb = d.textbbox((0, 0), fr, font=ff)
        d.text(((w - (bb[2] - bb[0])) / 2, 84), fr, font=ff, fill=fg)
        d.rectangle([0, 0, w - 1, h - 1], outline=tuple(max(0, c - 60) for c in bg), width=6)
        # dirt streaks at the bottom
        dark = Image.new("RGB", (w, h), (0, 0, 0))
        mask = Image.fromarray((np.linspace(0, 1, h)[:, None] ** 3 * 120 * np.ones((1, w))).astype(np.uint8))
        im = Image.composite(dark, im, mask)
        im.save(f"{OUT}/Signs/T_sign_{i:02d}.png")
    # PORT gate sign
    im = Image.new("RGB", (1024, 170), (20, 40, 30))
    d = ImageDraw.Draw(im)
    fa = ImageFont.truetype(KUFI, 70)
    ff = ImageFont.truetype(LATIN, 44)
    ta = arabic("ميناء وهران — مخرج")
    bb = d.textbbox((0, 0), ta, font=fa)
    d.text(((1024 - (bb[2] - bb[0])) / 2 - bb[0], 8 - bb[1]), ta, font=fa, fill=(120, 255, 160))
    d.text((300, 112), "PORT D'ORAN → SORTIE", font=ff, fill=(120, 255, 160))
    d.rectangle([0, 0, 1023, 169], outline=(90, 200, 120), width=8)
    im.save(f"{OUT}/Signs/T_sign_port.png")
    print("signs done")


if __name__ == "__main__":
    copy_pbr(); windows(); signs()
