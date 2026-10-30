"""Makes the splash art and the icon of the game from a capture of the map.

The splash shows the map of the year 196 with the banners of the rulers.
A card in the style of the panels of the game is at the centre. The card
has the title bar, the dark body, the game font and the track of the bar. The
platform draws the fill of the bar while the game loads
(HEXDOM_SPLASH_BAR_* in code/hexdom.h). The icon is one hex of the map
with the character 國 in the game font.

The capture is art-src/splash/map-196.png, out of git (docs/ASSETS.md,
"The splash art"). The script writes these files:

1. data/placeholder/sprites/art/splash.png, 1280 x 720, 8-bit RGB
2. web/splash.jpg, the same picture for the page of the web game
3. code/hexdom.ico, the icon of win32_hexdom.exe in seven sizes
4. web/favicon.png, the icon of the page, 64 x 64

Run it from the root of the repository:
    python -X utf8 misc\\make_splash.py
"""
import math
import os
import sys

from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CAPTURE = os.path.join(ROOT, "art-src", "splash", "map-196.png")
CJK_FONT = os.path.join(ROOT, "data", "fonts", "NotoSansCJKtc-Regular.otf")
LATIN_FONT = os.path.join(ROOT, "data", "fonts", "NotoSans-Regular.ttf")

WIDTH, HEIGHT = 1280, 720
# The part of the capture without the task bar of Windows, with the aspect 16:9.
CAPTURE_BOX = (40, 0, 1880, 1035)

# The colours of the panels: the dark style of Dear ImGui (ImGui::StyleColorsDark).
PANEL_BODY = (15, 15, 15, 255)   # opaque: the panel behind the card of the capture must not show
PANEL_TITLE = (41, 74, 122, 255)
PANEL_BORDER = (110, 110, 128, 128)
PANEL_FRAME = (29, 47, 73, 255)   # the frame colour of ImGui over the body of the card
TEXT = (255, 255, 255, 255)
TEXT_DISABLED = (128, 128, 128, 255)
BAR_FILL = (230, 179, 0, 255)

# The card covers the centre of the map. The bar must agree with
# HEXDOM_SPLASH_BAR_LEFT, _RIGHT, _TOP and _BOTTOM in code/hexdom.h.
CARD = (340, 200, 940, 540)
TITLE_HEIGHT = 34
BAR = (round(0.30 * WIDTH), round(0.65 * HEIGHT), round(0.70 * WIDTH), round(0.68 * HEIGHT))

ICON_SIZES = (16, 24, 32, 48, 64, 128, 256)
MAP_GREEN = (86, 140, 72, 255)
MAP_GREEN_DARK = (52, 92, 46, 255)


def centred_text(draw, centre_x, top, text, font, fill):
    left, _, right, _ = draw.textbbox((0, 0), text, font=font)
    draw.text((centre_x - (right - left) / 2 - left, top), text, font=font, fill=fill)


def make_background():
    capture = Image.open(CAPTURE).convert("RGB").crop(CAPTURE_BOX)
    picture = capture.resize((WIDTH, HEIGHT), Image.LANCZOS)
    # A dark edge puts the eye on the card.
    shade = Image.new("L", (WIDTH, HEIGHT), 0)
    ImageDraw.Draw(shade).ellipse((-WIDTH * 0.15, -HEIGHT * 0.25, WIDTH * 1.15, HEIGHT * 1.25), fill=255)
    shade = shade.filter(ImageFilter.GaussianBlur(120))
    dark = Image.new("RGB", (WIDTH, HEIGHT), (8, 9, 12))
    return Image.composite(picture, dark, shade.point(lambda value: 90 + value * 165 // 255))


def draw_card(picture):
    layer = Image.new("RGBA", picture.size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(layer)
    left, top, right, bottom = CARD
    draw.rectangle((left, top, right, bottom), fill=PANEL_BODY, outline=PANEL_BORDER)
    draw.rectangle((left + 1, top + 1, right - 1, top + TITLE_HEIGHT), fill=PANEL_TITLE)
    # The arrow and the name in the title bar, as in the title menu of the game.
    arrow_x, arrow_y = left + 12, top + 12
    draw.polygon([(arrow_x, arrow_y), (arrow_x + 12, arrow_y), (arrow_x + 6, arrow_y + 10)], fill=TEXT)
    draw.text((left + 34, top + 5), "hexdom", font=ImageFont.truetype(LATIN_FONT, 18), fill=TEXT)

    centre_x = (left + right) / 2
    centred_text(draw, centre_x, top + 58, "群雄割據", ImageFont.truetype(CJK_FONT, 84), TEXT)
    centred_text(draw, centre_x, top + 178, "一九六年  天子東遷", ImageFont.truetype(CJK_FONT, 26), TEXT_DISABLED)
    draw.line((left + 40, top + 236, right - 40, top + 236), fill=(60, 60, 68, 255), width=1)
    draw.rectangle(BAR, fill=PANEL_FRAME)
    return Image.alpha_composite(picture.convert("RGBA"), layer).convert("RGB")


def hex_points(size, inset):
    centre = size / 2
    radius = size / 2 - inset
    corners = []
    for index in range(6):
        angle = math.radians(60 * index - 90)   # a pointy top, as the hexes of the map
        corners.append((centre + radius * math.cos(angle), centre + radius * math.sin(angle)))
    return corners


def make_icon(size):
    scale = 4
    big = size * scale
    icon = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    draw = ImageDraw.Draw(icon)
    draw.polygon(hex_points(big, big * 0.02), fill=BAR_FILL)
    draw.polygon(hex_points(big, big * 0.08), fill=MAP_GREEN_DARK)
    draw.polygon(hex_points(big, big * 0.12), fill=MAP_GREEN)
    font = ImageFont.truetype(CJK_FONT, int(big * 0.62))
    left, top, right, bottom = draw.textbbox((0, 0), "國", font=font)
    at = ((big - (right - left)) / 2 - left, (big - (bottom - top)) / 2 - top)
    stroke = max(1, big // 40)
    draw.text(at, "國", font=font, fill=TEXT, stroke_width=stroke, stroke_fill=(20, 30, 20, 255))
    return icon.resize((size, size), Image.LANCZOS)


def main():
    if not os.path.isfile(CAPTURE):
        sys.exit("make_splash.py: no capture at " + CAPTURE)
    splash = draw_card(make_background())
    splash.save(os.path.join(ROOT, "data", "placeholder", "sprites", "art", "splash.png"), optimize=True)
    splash.save(os.path.join(ROOT, "web", "splash.jpg"), quality=70, optimize=True, progressive=True)

    icons = [make_icon(size) for size in ICON_SIZES]
    icons[-1].save(os.path.join(ROOT, "code", "hexdom.ico"), sizes=[(size, size) for size in ICON_SIZES])
    make_icon(64).save(os.path.join(ROOT, "web", "favicon.png"), optimize=True)
    print("make_splash.py: wrote the splash, the icon and the favicon")


if __name__ == "__main__":
    main()
