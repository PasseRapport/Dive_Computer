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
    image,z=frame(name)
    grey=lambda p: all(105<v<135 for v in p)
    for y in (149,161,173,185,197):
        assert pixels(name,(157,y,163,y+3),grey)==18, f'{name}: scuba dash missing'
    assert pixels(name,(150,145,170,202),white)==0, f'{name}: unexpected white arrow'
    assert pixels(name,(140,12,180,34),white)>15, f'{name}: battery header missing'

# Baseline option makes the changed geometry's red result reproducible.
if '--baseline' in sys.argv:
    geometry('30-underwater')
    raise AssertionError('Old Main unexpectedly meets the new geometry')

for name in ('apnea-01-fresh','apnea-04-descent','apnea-05-hold','apnea-06-ascent',
             'apnea-08-surface','apnea-12-long-recovery','apnea-17-long-x1','apnea-20-long-x2','apnea-25-reentry'):
    geometry(name)
# Surface right-hand timer and caption are absent throughout recovery.
for name in ('apnea-08-surface','apnea-09-red','apnea-10-green','apnea-11-expired','apnea-12-long-recovery'):
    assert pixels(name,(175,125,272,202),white)==0, f'{name}: surface timer still visible'

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
print('APNEA RENDER PASS: shared rails/battery, five scuba dashes and hidden surface timer, recovery ring lifecycle, decimal anchor, long values at x1/x2')
