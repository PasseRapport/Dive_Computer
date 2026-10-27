from PIL import Image
from pathlib import Path
import sys
root=Path(sys.argv[1])
def frame(n):return Image.open(root/(n+'.bmp')).convert('RGB').resize((620,420)).crop((22,32,342,332))
white=lambda p:min(p)>220
gray=lambda p:40<=min(p)<=60 and max(p)-min(p)<10
stop=frame('scuba-10-safety-stop');normal=frame('scuba-04-main')
for im in (stop,normal):
 n=sum(gray(im.getpixel((x,y))) for y in range(39,107) for x in range(52,268))
 assert n<1500,('Gray depth band remains',n)
markers=[]
for x in range(50,110):
 if all(white(stop.getpixel((x+i,y))) for y in (68,89) for i in range(1,15)) and all(white(stop.getpixel((x+i,y))) for y in (75,82) for i in (7,8)):markers.append(x)
assert markers,'Depth triangles must point towards each other'
ceiling=frame('scuba-13-deco')
rows=[y for y in range(239,287) if sum(white(ceiling.getpixel((x,y))) for x in range(75,245))>8]
assert max(rows)-min(rows)>=28,'Ceiling depth has not been enlarged'
acked=frame('scuba-15-alert-acknowledged')
red=lambda p:p[0]>150 and p[0]>p[1]*1.5 and p[0]>p[2]*1.5
assert not any(red(acked.getpixel((x,y))) for y in range(34,46) for x in range(90,230)),'Alert text remains after acknowledgement'
pending=frame('scuba-14-ceiling-alert')
orange=lambda p:p[0]>150 and 70<p[1]<200 and p[2]<110
assert not any(orange(pending.getpixel((x,y))) for y in range(277,287) for x in range(130,190)),'Acknowledgement hint remains visible'

# A short alert stays inside the circular screen at this latitude.
alert=frame('scuba-13-deco')
red_pixels=[(x,y) for x in range(320) for y in range(34,46) if red(alert.getpixel((x,y)))]
assert red_pixels and min(x for x,y in red_pixels)>=64 and max(x for x,y in red_pixels)<=256,'Decompression alert exceeds circular safe width'
print('STOP DISPLAY PASS: no gray band, inward stop triangles, larger ceiling and no acknowledged alert text')
