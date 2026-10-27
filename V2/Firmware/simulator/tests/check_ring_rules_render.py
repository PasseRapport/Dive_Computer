from pathlib import Path
from PIL import Image
import math,sys
root=Path(sys.argv[1])
def counts(name):
 im=Image.open(root/(name+'.bmp')).convert('RGB').resize((620,420)).crop((22,32,342,332));green=orange=0
 for y in range(300):
  for x in range(320):
   if math.hypot(x-159.5,y-149.5)<136:continue
   r,g,b=im.getpixel((x,y));green+=g>120 and g>r*1.5 and g>b*1.25;orange+=r>150 and 70<g<200 and b<100
 return green,orange
full=counts('rules-ndl-60');over=counts('rules-ndl-over60');five=counts('rules-ndl-five');under=counts('rules-ndl-under-five');zero=counts('rules-ndl-zero')
assert full[0]>5000 and abs(full[0]-over[0])<30,(full,over)
assert five[0]>400 and five[1]==0,five
assert under[1]>400 and under[0]==0,under
assert sum(zero)==0,zero
print('RING BOUNDARY RENDER PASS: full at 60/over 60; green at 5; orange below 5; empty at zero')
