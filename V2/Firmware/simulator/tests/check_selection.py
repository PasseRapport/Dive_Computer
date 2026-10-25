"""Rendered acceptance checks for fitted capsules and the white outline."""
from pathlib import Path
import sys
from PIL import Image

root = Path(sys.argv[1])

def capsule(name):
    image = Image.open(root / (name + '.bmp')).convert('RGB')
    zoom = image.width // 620
    pixels = []
    blue = 0
    for y in range(300):
        for x in range(320):
            red, green, blue_channel = image.getpixel(((22+x)*zoom, (32+y)*zoom))
            if green > 90 and blue_channel > 90 and red < 45 and abs(green-blue_channel) < 30:
                pixels.append((x, y))
            if blue_channel > 180 and blue_channel > red + 70 and blue_channel > green + 40:
                blue += 1
    assert pixels, f'{name}: missing selection capsule'
    assert blue == 0, f'{name}: blue outline is still visible'
    left, right = min(x for x,y in pixels), max(x for x,y in pixels)
    top = min(y for x,y in pixels)
    middle = (left+right)//2
    outline = [image.getpixel(((22+middle)*zoom, (32+top-offset)*zoom)) for offset in (1,2,3)]
    assert any(min(pixel) > 200 for pixel in outline), f'{name}: outline is not white'
    print(f'{name}: fitted capsule width {right-left+1}px, white outline')
    return right-left+1

capsule('01-menu-diving')
brightness = capsule('04-option-brightness')
sleep = capsule('07-option-sleep')
assert brightness > sleep + 40, 'Option capsule does not follow label width'
scuba = capsule('16-mode-scubadiving')
free = capsule('17-mode-freediving')
assert scuba > free + 4, 'Dive-mode capsule does not follow label width'
fresh = capsule('19-water-fresh')
marine = capsule('20-water-marine')
assert marine > fresh + 5, 'Water-type capsule does not follow icon + label bounds'
capsule('24-option-bluetooth')
for name in ['44-backlight-menu', '45-backlight-auto', '25-bluetooth-off', '48-bluetooth-dfu', '73-options-haptics', '74-haptics-on', '75-haptics-off', '76-haptics-long', '77-haptics-short']:
    if (root / (name + '.bmp')).exists():
        capsule(name)
print('SELECTION CHECK PASS: adaptive width and white borders in every selector')
