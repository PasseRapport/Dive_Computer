"""Inspect rendered scuba states, perimeter rings and persistent alerts."""
from pathlib import Path
import sys
from PIL import Image
root=Path(sys.argv[1])
def pixels(name):
    image=Image.open(root/(name+'.bmp')).convert('RGB')
    zoom=image.width//620
    return image.resize((620,420)).crop((22,32,342,332))
def count(image,test,box=(0,0,320,300)):
    cropped=image.crop(box)
    return sum(test(*cropped.getpixel((x,y))) for y in range(cropped.height) for x in range(cropped.width))
white=lambda r,g,b:min(r,g,b)>190
teal=lambda r,g,b:r<70 and g>100 and b>100
green=lambda r,g,b:g>120 and g>r*1.5 and g>b*1.25
red=lambda r,g,b:r>150 and r>g*1.5 and r>b*1.5
orange=lambda r,g,b:r>150 and 70<g<200 and b<100
for name in ['scuba-01-ready','scuba-02-settings','scuba-03-depth-alarm','scuba-04-main','scuba-05-compass','scuba-06-stats','scuba-07-brightness','scuba-08-surface','scuba-09-leave-quarter','scuba-10-safety-stop','scuba-11-stop-complete','scuba-12-surface-exit','scuba-13-deco','scuba-14-ceiling-alert','scuba-15-alert-acknowledged','scuba-16-low-battery','scuba-17-summary']:
    assert count(pixels(name),white)>80,f'{name}: missing screen text'
assert count(pixels('scuba-02-settings'),teal)>200,'Missing fitted settings capsule'
assert count(pixels('scuba-04-main'),teal)>1000,'Missing NDL perimeter'
assert count(pixels('scuba-05-compass'),white,(95,78,225,120))>120,'Missing enlarged depth above azimuth'
assert count(pixels('scuba-05-compass'),white,(95,185,225,227))>120,'Missing enlarged time below azimuth'
assert count(pixels('scuba-04-main'),white,(70,28,250,60))==0,'Scuba heading is still visible'
assert count(pixels('scuba-04-main'),white,(70,148,250,182))>150,'Dive time is not enlarged' 
assert count(pixels('scuba-07-brightness'),teal,(60,130,260,158))>40,'Missing brightness bar'
assert count(pixels('scuba-10-safety-stop'),teal)>300,'Safety-stop progress not rendered'
assert count(pixels('scuba-11-stop-complete'),green)>2000,'Completed stop does not fill the ring green'
assert count(pixels('scuba-14-ceiling-alert'),red,(40,25,280,52))>20,'Missing ceiling alert'
assert count(pixels('scuba-15-alert-acknowledged'),red,(40,25,280,52))>20,'Acknowledgement removed persistent warning'
print('SCUBA RENDER PASS: preparation/settings, NDL ring, compass metrics, brightness, stop progress/completion, persistent alerts and summary')
