"""Check rendered native Main regions; no widget identities or whole-frame snapshots."""
from pathlib import Path
import math
import sys
from PIL import Image
root = Path(sys.argv[1])
def frame(name):
    image = Image.open(root / (name + '.bmp')).convert('RGB')
    zoom = image.width // 620
    return image, zoom

def pixels(name, box, predicate):
    image, zoom = frame(name)
    x1,y1,x2,y2 = box
    return sum(predicate(image.getpixel(((22+x)*zoom,(32+y)*zoom)))
               for y in range(y1,y2) for x in range(x1,x2))
white = lambda p: min(p)>220
cyan = lambda p: p[0]<90 and p[1]>180 and p[2]>180

def geometry(name):
    for y in (118,215):
        assert pixels(name,(100,y,220,y+1),cyan)>110, f'{name}: missing common cyan separator at {y}'
    assert pixels(name,(159,160,162,185),white)>65, f'{name}: central white stroke missing'
    assert pixels(name,(140,12,180,34),white)>15, f'{name}: battery header missing'

# Baseline option makes the changed geometry's red result reproducible.
if '--baseline' in sys.argv:
    geometry('30-underwater')
    raise AssertionError('Old Main unexpectedly meets the new geometry')

for name in ('apnea-01-fresh','apnea-04-descent','apnea-05-hold','apnea-06-ascent',
             'apnea-08-surface','apnea-12-long-recovery','apnea-17-long-x1','apnea-20-long-x2','apnea-25-reentry'):
    geometry(name)
# Indicator chevrons occupy distinct regions outside the permanent stroke.
for name, upper, lower in (
    ('apnea-04-descent',False,True),('apnea-05-hold',False,False),
    ('apnea-06-ascent',True,False),('apnea-08-surface',False,False),
    ('apnea-09-red',False,False),('apnea-12-long-recovery',False,False)):
    assert (pixels(name,(151,145,170,154),white)>20)==upper, f'{name}: wrong ascent chevron'
    assert (pixels(name,(151,193,170,202),white)>20)==lower, f'{name}: wrong descent chevron'

def ring(name):
    image,z=frame(name);red=green=0
    for degrees in range(0,360,5):
        a=math.radians(degrees)
        r,g,b=image.getpixel((round((22+160+144*math.cos(a))*z),round((32+150+144*math.sin(a))*z)))
        red+=r>200 and g<60 and b<60
        green+=g>200 and r<60 and b<60
    return red,green
assert ring('apnea-09-red')[0]>65
assert ring('apnea-10-green')[1]>=70
for name in ('apnea-01-fresh','apnea-04-descent','apnea-11-expired','apnea-12-long-recovery',
             'apnea-14-statistics-no-ring','apnea-25-reentry'):
    assert ring(name)==(0,0), f'{name}: unexpected recovery ring'
# Same anchored decimal bitmap for one-, two- and three-digit whole depths.
for name in ('apnea-04-descent','apnea-06-ascent','apnea-17-long-x1','apnea-20-long-x2'):
    assert pixels(name,(157,89,164,101),white)>8, f'{name}: decimal anchor moved'
# No text from the centre blocks may invade the central direction gutter.
for name in ('apnea-17-long-x1','apnea-20-long-x2'):
    assert pixels(name,(146,154,150,191),white)==0, f'{name}: left block crosses gutter'
    assert pixels(name,(171,154,173,191),white)==0, f'{name}: timer crosses gutter'
print('APNEA RENDER PASS: shared rails/battery, fixed stroke and three directions, recovery ring lifecycle, decimal anchor, long values at x1/x2')
