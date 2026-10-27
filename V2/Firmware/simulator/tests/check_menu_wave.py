"""Rendered checks for accumulated decompression progress and summary layout."""
from pathlib import Path
from PIL import Image
import sys,math
root=Path(sys.argv[1])
def frame(n):return Image.open(root/(n+'.bmp')).convert('RGB').resize((620,420)).crop((22,32,342,332))
def amount(im,test,box):return sum(test(*im.getpixel((x,y))) for y in range(box[1],box[3]) for x in range(box[0],box[2]))
def arc_color(r,g,b):return g>100 and (r>160 and b<110 or g>r*1.5 and g>b*1.25)
progress=[]
for name in ['zero','first','second','complete']:
 im=frame('wave-deco-'+name)
 progress.append(sum(arc_color(*im.getpixel((x,y))) for y in range(300) for x in range(320) if math.hypot(x-159.5,y-149.5)>136))
assert progress[0]>progress[1] and abs(progress[1]-progress[2])<50 and progress[3]<10,progress
white=lambda r,g,b:min(r,g,b)>170
for name in ['first','later']:
 im=frame('wave-summary-'+name)
 assert amount(im,white,(70,233,250,261))>100,'Surface time missing from summary'
 assert amount(im,white,(70,190,250,220))>100,'Duration missing from summary'
a=frame('wave-summary-first');b=frame('wave-summary-later')
assert sum(a.getpixel((x,y))!=b.getpixel((x,y)) for y in range(233,261) for x in range(70,250))>15,'Surface time did not update'
print('MENU WAVE RENDER PASS: stop ring follows remaining time from full to empty; live summary surface time and duration')

im=frame('wave-main-menu')
gray=lambda r,g,b:120<min(r,g,b)<150 and max(r,g,b)-min(r,g,b)<12
assert amount(im,gray,(40,100,285,260))>300,'Unselected menu rows are not gray'
assert amount(im,white,(100,40,265,90))>100,'Selected menu row is not white'
im=frame('wave-backlight')
assert amount(im,white,(60,45,270,80))>100,'Menu heading is not white'
assert amount(im,gray,(60,145,270,235))>100,'Inactive backlight rows are not gray'
print('MENU COLOUR PASS: white headings and selected rows, gray inactive rows')
