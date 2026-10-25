"""Verify the rendered recovery ring over an actual SDL-driven simulated dive."""
from pathlib import Path
import math
import sys
from PIL import Image
root = Path(sys.argv[1])

def counts(name):
    image = Image.open(root / (name + '.bmp')).convert('RGB')
    zoom = image.width // 620
    red = green = 0
    for degrees in range(0, 360, 5):
        angle = math.radians(degrees)
        x = round((22 + 160 + 144*math.cos(angle))*zoom)
        y = round((32 + 150 + 144*math.sin(angle))*zoom)
        r,g,b = image.getpixel((x,y))
        red += r > 200 and g < 60 and b < 60
        green += g > 200 and r < 60 and b < 60
    print(f'{name}: red={red}/72, green={green}/72')
    return red,green
assert counts('30-underwater') == (0,0), 'Ring must be hidden during the dive'
r1,g1 = counts('31-recovery-red')
r2,g2 = counts('32-recovery-red-later')
assert r1 > 5 and r2 > r1 + 10 and g1 == g2 == 0, 'Red ring must fill during recovery'
for name in ['33-recovery-green','34-recovery-green-hold']:
    red,green = counts(name)
    assert green >= 70 and red == 0, 'Completed ring must remain fully green'
assert counts('35-recovery-ring-hidden') == (0,0), 'Ring must disappear after the 10-second green hold'
print('RECOVERY RING CHECK PASS: progressive red, full green hold and expiry')
