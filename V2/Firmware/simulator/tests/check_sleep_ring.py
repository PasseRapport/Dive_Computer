"""Check the sleep ring is drawn at the actual circular screen edge."""
from pathlib import Path
import math
import sys
from PIL import Image
root = Path(sys.argv[1])

def ring(name):
    image = Image.open(root / (name + '.bmp')).convert('RGB')
    zoom = image.width // 620
    values = []
    for degrees in range(0, 360, 5):
        angle = math.radians(degrees)
        x = 160 + 145 * math.cos(angle)
        y = 150 + 145 * math.sin(angle)
        pixel = image.getpixel((round((22+x)*zoom), round((32+y)*zoom)))
        values.append(min(pixel) > 180)
    return values
quarter = ring('22-sleep-quarter')
full = ring('23-sleep-almost-full')
assert 12 <= sum(quarter) <= 23, f'Quarter ring is missing or clipped: {sum(quarter)}/72 samples'
assert sum(full) >= 65, f'Almost complete ring is clipped: {sum(full)}/72 samples'
assert quarter[20] and quarter[32] and not quarter[2] and not quarter[50], 'Ring must start at bottom and follow the circular edge'
assert not any(ring('15-display-off')), 'Ring remains on screen after sleep'
image = Image.open(root / '22-sleep-quarter.bmp').convert('RGB')
zoom = image.width // 620
white_width = 0
for radius in range(135,150):
    angle = math.radians(135)
    pixel = image.getpixel((round((22+160+radius*math.cos(angle))*zoom),
                            round((32+150+radius*math.sin(angle))*zoom)))
    white_width += min(pixel) > 200
assert white_width >= 8, f'Sleep ring was not thickened: {white_width}px'
if (root / '41-scuba-leave-quarter.bmp').exists():
    assert 12 <= sum(ring('41-scuba-leave-quarter')) <= 23, 'Scuba exit ring missing'
    assert sum(ring('43-free-leave-almost-full')) >= 65, 'Freediving exit ring missing or clipped'
    print('DIVE EXIT RING CHECK PASS: both dive modes use the complete circular perimeter')
if (root / '61-watch-leave-quarter.bmp').exists():
    assert 12 <= sum(ring('61-watch-leave-quarter')) <= 23, 'Watch exit ring missing'
    assert sum(ring('64-compass-leave-almost-full')) >= 65, 'Compass exit ring missing or clipped'
    print('WATCH/COMPASS EXIT CHECK PASS: confirmation rings reach the display perimeter')
print('SLEEP RING CHECK PASS: bottom origin, quarter/full perimeter and display-off')
