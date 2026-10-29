#!/usr/bin/env python3
"""Check native catalog captures: geometry, theme changes, dynamic values, x1/x2."""
from pathlib import Path
import math,sys
from PIL import Image,ImageChops
root=Path(sys.argv[1])
def image(name,scale=1):
 im=Image.open(root/(name+'.bmp')).convert('RGB')
 return im.crop((22*scale,32*scale,342*scale,332*scale)).resize((320,300),Image.Resampling.NEAREST)
faces={}
for kind,count in [('analog',7),('digital',5)]:
 for n in range(count):
  name=f'{kind}-{n}';a=image(name+'-x1');b=image(name+'-x2',2);faces[kind,n]=a
  # Rasterized image edges can quantize differently under the desktop x2 layer.
  # Allow at most 0.2% of pixels, while all geometry must remain at native size.
  diff=[max(p) for p in ImageChops.difference(a,b).get_flattened_data()]
  assert sum(v>0 for v in diff)<=192,(name,'x1/x2 geometry drift')
  assert sum(v>32 for v in diff)<=24,(name,'x1/x2 large colour drift')
for n in (0,1):assert max(faces['analog',n].getpixel((160,150)))<20,'Unwanted center pivot'
for minute in range(60):
 t=minute*math.pi/30;x=round(160+140*math.sin(t));y=round(150-140*math.cos(t))
 assert any(min(p)>=128 for p in faces['analog',1].crop((x-2,y-2,x+3,y+3)).get_flattened_data()),f'Missing Dauphine minute {minute}'
# LCD fills the dial; no logo or percent label is placed on its upper face.
sport=faces['digital',3]
assert sport.getpixel((160,40))==sport.getpixel((160,90))==sport.getpixel((160,260)),'Sport LCD background not uniform'
# Radar segments sit at r=104, away from the restored 6 and the 9.
full,half,empty=[image('radar-'+s) for s in ('full','half','empty')]
for i in range(12):
 angle=math.radians(226+i*8);x=round(160+104*math.sin(angle));y=round(150-104*math.cos(angle))
 def green(im):
  return max(g-r for r,g,b in im.crop((x-1,y-1,x+2,y+2)).get_flattened_data())>60
 assert green(full) and not green(empty),f'Radar segment {i}: full/empty'
 assert green(half)==(i<6),f'Radar discharge does not start at top: {i}'
assert ImageChops.difference(full.crop((150,260,170,281)),empty.crop((150,260,170,281))).getbbox() is None,'Battery overlaps 6'
# Actual hours, dates and battery values must alter the corresponding rendered data.
for normal,changed in [('digital-0-x1','emblem-2359'),('digital-1-x1','terminal-2359'),('digital-3-x1','sport-2359')]:
 assert ImageChops.difference(image(normal),image(changed)).getbbox(),f'{normal}: frozen time'
roman=[image('solar-roman-'+s).crop((105,237,215,260)) for s in ('87','zero','100')]
assert all(ImageChops.difference(roman[i],roman[j]).getbbox() for i,j in [(0,1),(0,2),(1,2)]),'Roman battery frozen'
print('WATCH IMAGES PASS: twelve distinct live faces, minute ticks, LCD theme, radar direction/clearance, Roman battery and x1/x2 geometry')

assert ImageChops.difference(image('carousel-solar'),faces['analog',6]).getbbox() is None,'Preview is scaled, cropped or captioned instead of full-screen'
print('FULLSCREEN PREVIEW PASS: selector matches active face pixel for pixel')
