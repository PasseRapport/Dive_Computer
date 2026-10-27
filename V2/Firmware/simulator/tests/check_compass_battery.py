from pathlib import Path
from PIL import Image,ImageChops
import sys
root=Path(sys.argv[1])
def frame(name):return Image.open(root/(name+'.bmp')).convert('RGB').resize((620,420)).crop((22,32,342,332))
normal=frame('scuba-51').crop((142,16,178,32))
for name in ('compass-ring-north','compass-ring-east'):
 im=frame(name)
 assert ImageChops.difference(normal,im.crop((142,30,178,46))).getbbox() is None,(name,'Battery moved or compass overlaps its reserved area')
 assert sum(min(im.getpixel((x,y)))>180 for y in range(219,235) for x in range(244,280))<25,(name,'Battery remains at lower right')
print('COMPASS BATTERY PASS: lowered upper battery, no compass overlap, no lower-right duplicate')

north=frame('compass-ring-north')
assert sum(min(north.getpixel((x,y)))>180 for x in range(151,168) for y in range(10,25))>8,'North must remain above battery'
assert any(r>180 and g<110 and b<110 for r,g,b in [north.getpixel((x,y)) for x in range(157,164) for y in range(2,13)]),'North tick must be red'
