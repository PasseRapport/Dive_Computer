from pathlib import Path
from PIL import Image
import sys
root=Path(sys.argv[1])
def frame(name):return Image.open(root/(name+'.bmp')).convert('RGB').resize((620,420)).crop((22,32,342,332))
def count(im,box,test):return sum(test(*im.getpixel((x,y))) for y in range(box[1],box[3]) for x in range(box[0],box[2]))
white=lambda r,g,b:min(r,g,b)>180
red=lambda r,g,b:r>100 and r>g*2 and r>b*2
yellow=lambda r,g,b:r>100 and g>100 and b<80
for percent,test in [(0,red),(10,red),(11,yellow),(50,yellow),(51,white),(87,white)]:
 im=frame('digital-'+str(percent))
 assert count(im,(141,237,179,255),test)>30,f'Battery colour at {percent}%'
 assert count(im,(120,202,200,225),white)>70,'Date missing'
 assert count(im,(130,45,190,101),white)>180,'Digital logo missing'
assert count(frame('digital-0'),(146,243,168,249),red)==0,'Empty battery filled'
assert count(frame('digital-87'),(146,243,168,249),white)>80,'Charged battery fill missing'
for name,test,box in [('analog-87',white,(141,217,179,235)),('freedive-87',white,(141,34,179,52)),('freedive-10',red,(141,34,179,52)),('scuba-87',white,(141,15,179,33)),('scuba-50',yellow,(141,15,179,33)),('scuba-10',red,(141,15,179,33)),('scuba-51',white,(141,15,179,33))]:
 assert count(frame(name),box,test)>30,name+': battery icon missing'
assert count(frame('analog-87'),(199,140,267,160),white)>70,'Analog date missing'
assert count(frame('analog-87'),(130,62,190,118),white)>180,'Analog logo missing'
print('APPEARANCE RENDER PASS: colours 0/10/11/50/51/87%, proportional fill, dates, logos, battery on all faces')
