"""Rendered regression checks for the scuba dashboard refinements."""
from pathlib import Path
import math,sys
from PIL import Image
root=Path(sys.argv[1])
def frame(name):
 return Image.open(root/(name+'.bmp')).convert('RGB').resize((620,420)).crop((22,32,342,332))
cyan=lambda r,g,b:r<100 and g>70 and b>70 and abs(g-b)<30 and g>r*1.8
red=lambda r,g,b:r>100 and r>g*2 and r>b*2
im=frame('scuba-87')
def ends(y):
 points=[(x,j) for j in range(y,y+10) for x in range(30,290) if cyan(*im.getpixel((x,j)))]
 return min(points),max(points)
top,bottom=ends(118),ends(215)
assert bottom[0][0]>top[0][0] and bottom[1][0]<top[1][0], 'Lower separator is not shorter'
rt=math.hypot(top[0][0]-159.5,top[0][1]-149.5)
rb=math.hypot(bottom[0][0]-159.5,bottom[0][1]-149.5)
assert abs(rt-rb)<1.2, f'Unequal circular edge margins: {rt}, {rb}'
for i in range(5):
 y0=194-12*i
 for y in range(y0,y0+8):
  for x in range(150,170):
   r,g,b=im.getpixel((x,y))
   if max(r,g,b)>45:
    assert 157<=x<=162 and y0+3<=y<=y0+5, 'Arrow visible while not ascending'
# Rounded cap extends a few pixels past the start angle, within the ring width.
r,g,b=im.getpixel((162,293))
assert g>120 and g>r*1.5 and g>b*1.25, 'The ring start cap is still flat'
im=frame('scuba-10')
assert sum(red(*im.getpixel((x,y))) for y in range(15,33) for x in range(141,179))>30,'Low battery not at the top'
assert not any(red(*im.getpixel((x,y))) for y in range(34,46) for x in range(60,260)), 'Redundant low battery caption remains'
print('DIVE REFINEMENT PASS: shorter lower rail, equal circular margins, idle dashes, rounded cap and top battery without redundant text')
