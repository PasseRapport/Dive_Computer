"""The decimal stays fixed across single-, double- and triple-digit depths."""
from pathlib import Path
from PIL import Image
import sys
root=Path(sys.argv[1]);dots=[]
for minute in (9,10,52,99,100,999):
 im=Image.open(root/f'timer-{minute}.bmp').convert('RGB').resize((620,420)).crop((22,32,342,332))
 dot={(x,y) for x in range(154,167) for y in range(91,104) if min(im.getpixel((x,y)))>220}
 assert len(dot)>20,(minute,'Decimal point missing')
 assert abs((min(x for x,y in dot)+max(x for x,y in dot))/2-160)<=0.5,(minute,'Decimal is not centred')
 dots.append(dot)
assert all(dot==dots[0] for dot in dots),'Depth decimal moved with integer digits'
print('DEPTH ANCHOR PASS: stable decimal at 0.0/9.9/10.0/31.1/99.9/100.0 m')
