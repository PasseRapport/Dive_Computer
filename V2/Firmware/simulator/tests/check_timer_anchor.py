from pathlib import Path
from PIL import Image
import sys
root=Path(sys.argv[1]);edges=[]
for minute in (9,10,52,99,100,999):
 im=Image.open(root/f'timer-{minute}.bmp').convert('RGB').resize((620,420)).crop((22,32,342,332))
 points={(x,y) for x in range(173,310) for y in range(150,205) if min(im.getpixel((x,y)))>220};large=[]
 assert not any(x>=285 for x,y in points),(minute,'Dive time overflows safe circular area')
 assert sum(x>=242 for x,y in points)>40,(minute,'Seconds are missing or overlap the minutes')
 while points:
  seed=points.pop();todo=[seed];part=[seed]
  while todo:
   x,y=todo.pop()
   for n in ((x-1,y),(x+1,y),(x,y-1),(x,y+1)):
    if n in points:points.remove(n);todo.append(n);part.append(n)
  if max(y for x,y in part)-min(y for x,y in part)>=22:large.extend(part)
 assert large,(minute,'Missing large minutes')
 edge=max(x for x,y in large);edges.append(edge)
 assert 233<=edge<=240,(minute,'Minute units must stay at their fixed right anchor',edge)
assert max(edges)-min(edges)<=4,('Minute units moved',edges)
print('TIMER RENDER PASS: fixed minute-units anchor and complete time within circular display for 9/10/52/99/100/999 min')
