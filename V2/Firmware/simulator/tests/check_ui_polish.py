"""Pixel checks for the requested watch layout and symmetric dive geometry."""
from PIL import Image
from pathlib import Path
import sys,math
root=Path(sys.argv[1])
def frame(name):return Image.open(root/(name+'.bmp')).convert('RGB').resize((620,420)).crop((22,32,342,332))
def count(im,box,test):return sum(test(*im.getpixel((x,y))) for y in range(box[1],box[3]) for x in range(box[0],box[2]))
white=lambda r,g,b:min(r,g,b)>180
cyan=lambda r,g,b:r<100 and g>70 and b>70 and abs(g-b)<30 and g>r*1.8
im=frame('digital-87')
rows=[y for y in range(240,252) if white(*im.getpixel((150,y)))]
assert rows==list(range(240,252)),f'Battery fill not aligned: {rows}'
assert count(im,(130,45,190,101),white)>300,'Digital upper logo missing'
assert count(im,(141,237,179,255),white)>80,'Digital battery not below date'
analog=frame('analog-87')
assert count(analog,(199,140,267,160),white)>70,'Analog day/date not at three oclock'
assert count(analog,(158,120,162,135),white)==0,'Seconds hand still visible'
assert count(analog,(130,62,190,118),white)>300,'Enlarged analog logo missing'
for name in ['general-brightness','general-sensitivity-half']:
 im=frame(name)
 assert count(im,(70,135,250,153),cyan)>400,'Missing shared blue bar'
 assert count(im,(70,85,250,120),white)==0,'Numeric light value still visible'
for name,box in [('compass-ring-north',(151,10,168,28)),('compass-ring-east',(20,141,38,160))]:
 im=frame(name)
 assert count(im,box,white)>8,'North label does not rotate on perimeter'
 assert count(im,(110,50,220,105),white)>100,'Compass ring replaced central dive data'
im=frame('scuba-87')
assert all(im.getpixel((x,y))==im.getpixel((319-x,y)) for y in range(194,202) for x in range(150,170)),'Ascent chevron is asymmetric'
for y0 in [118,215]:
 assert all(cyan(*im.getpixel((x,y)))==cyan(*im.getpixel((319-x,y))) for y in range(y0,y0+10) for x in range(40,280)),'Blue separators are asymmetric'
green=lambda r,g,b:g>120 and g>r*1.5 and g>b*1.25
radius=max(math.hypot(x-159.5,y-149.5) for y in range(300) for x in range(320) if green(*im.getpixel((x,y))))
assert radius<148.6,f'Perimeter protrusion at radius {radius}'
print('UI POLISH RENDER PASS: aligned battery, watch layout, compass, shared light bars, exact chevron/separator symmetry and clean perimeter')
