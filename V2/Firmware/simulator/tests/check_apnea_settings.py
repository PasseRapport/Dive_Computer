"""Public captures: centred scuba-style selectors at x1/x2, no settings recovery ring."""
from pathlib import Path
import sys
from PIL import Image
root = Path(sys.argv[1])
for name, zoom in [('settings-20', 1), ('settings-29', 1), ('settings-bottom-x2', 2)]:
    frame = Image.open(root / (name + '.bmp')).convert('RGB')
    assert frame.width == 620 * zoom, (name, 'wrong scale')
    watch = frame.crop((22*zoom, 32*zoom, 342*zoom, 332*zoom))
    ink = []
    for y in range(300):
        for x in range(320):
            r, g, b = watch.getpixel((x*zoom, y*zoom))
            if g > 90 and b > 90 and r < 45 and abs(g-b) < 30:
                ink.append((x,y))
            if (x-160)**2 + (y-150)**2 > 135**2:
                assert not (r > 160 and g < 90 and b < 90), (name, 'red recovery ring in Settings')
                assert not (g > 160 and r < 90 and b < 90), (name, 'green recovery ring in Settings')
    assert ink, (name, 'missing fitted selection capsule')
    left, right = min(x for x,y in ink), max(x for x,y in ink)
    top, bottom = min(y for x,y in ink), max(y for x,y in ink)
    assert left >= 20 and right <= 300, (name, 'capsule clipped')
    if name != 'settings-20':
        assert abs((left+right)/2 - 160) <= 3, (name, 'capsule off centre')
        assert abs((top+bottom)/2 - 150) <= 5, (name, 'carousel selection not centred')
    print(name, 'PASS: fitted water capsule' if name == 'settings-20' else 'PASS: centred carousel capsule', 'and no recovery ring')
print('APNEA SETTINGS RENDER PASS: x1/x2 public images')
