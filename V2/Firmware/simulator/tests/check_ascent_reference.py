"""Pixel acceptance checks for the requested ascent-bar reference."""
from pathlib import Path
from PIL import Image
import sys
root=Path(sys.argv[1])
def frame(name):return Image.open(root/('ascent-'+name+'.bmp')).convert('RGB').resize((620,420)).crop((22,32,342,332))
colors={
 'gray':lambda r,g,b:90<min(r,g,b) and max(r,g,b)<150,
 'green':lambda r,g,b:g>180 and g>r*2 and g>b*1.3,
 'yellow':lambda r,g,b:r>200 and g>180 and b<120,
 'red':lambda r,g,b:r>180 and g<110 and b<110,
 'white':lambda r,g,b:min(r,g,b)>220}
for name,levels,color in [('descending',0,'gray'),('hold',0,'gray'),('below-one',0,'gray'),('one',1,'gray'),('under-two',1,'gray'),('two',1,'green'),('four',2,'green'),('six',3,'green'),('eight',4,'green'),('above-eight',4,'yellow'),('ten',5,'yellow'),('above-ten',5,'red'),('highlight',5,'white')]:
 im=frame(name)
 for i in range(5):
  y=194-12*i
  if i<levels:
   assert colors[color](*im.getpixel((159,y))),f'{name}: wrong arrow {i+1}'
  else:
   assert max(im.getpixel((159,y)))<50,f'{name}: unexpected arrow {i+1}'
   assert colors['gray'](*im.getpixel((159,y+4))),f'{name}: missing inactive dash'
 if name=='highlight':assert colors['red'](*im.getpixel((149,172))),'Missing highlighted red background'
 else:assert max(im.getpixel((149,172)))<50,'Premature red background'
print('ASCENT REFERENCE RENDER PASS: descent/<1 dashes, gray 1-2, green 2-8, yellow >8-10, red >10 and white/red at 5 seconds')
