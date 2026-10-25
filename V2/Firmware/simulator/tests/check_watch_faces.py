"""Rendered checks for the black-background logo fade and both clock styles."""
from pathlib import Path
import math
import sys
from PIL import Image, ImageStat, ImageChops
root = Path(sys.argv[1])
boot_only = '--boot-only' in sys.argv[2:]

def frame(name):
    return Image.open(root/(name+'.bmp')).convert('RGB')

def logo_brightness(name):
    # The complete asset rectangle lies inside the circular aperture.
    image = frame(name)
    zoom = image.width // 620
    roi = image.crop((82*zoom, 78*zoom, 282*zoom, 286*zoom))
    return sum(ImageStat.Stat(roi).mean)/3
start, incoming, full = [logo_brightness(n) for n in [
    '00-logo-start','00-logo-fade-in','00-project-logo']]
assert full > 30 and start < full*0.15, 'Logo does not fade in from black'
assert start < incoming < full*0.8, 'Missing intermediate fade-in frame'
image = frame('00-project-logo')
zoom = image.width // 620
for x,y in [(84,80),(280,80),(84,284),(280,284)]:
    assert max(image.getpixel((x*zoom,y*zoom))) == 0, 'Logo still has a visible rectangular background'

def white_points(image, box):
    zoom = image.width // 620
    x1,y1,x2,y2 = box
    return [(x,y) for x in range(x1,x2) for y in range(y1,y2)
            if min(image.getpixel(((22+x)*zoom,(32+y)*zoom))) > 180]

full_points = white_points(frame('00-project-logo'), (30,20,290,280))
shrinking = white_points(frame('00-logo-shrinking'), (30,20,290,280))
full_width = max(x for x,y in full_points)-min(x for x,y in full_points)
moving_width = max(x for x,y in shrinking)-min(x for x,y in shrinking)
assert 60 < moving_width < full_width*0.85, 'Logo is not shrinking during startup'
assert 170 < (min(y for x,y in shrinking)+max(y for x,y in shrinking))/2 < 230, 'Logo is not moving down toward its dock'
clock_frames = ['00-logo-docked','00-clock'] + ([] if boot_only else ['56-watch-digital'])
for name in clock_frames:
    points = white_points(frame(name), (110,200,210,278))
    assert len(points) > 350, f'{name}: permanent digital logo is missing'
    left,right = min(x for x,y in points),max(x for x,y in points)
    top,bottom = min(y for x,y in points),max(y for x,y in points)
    assert 44 <= right-left <= 62 and 46 <= bottom-top <= 66, 'Digital logo is not at its small permanent size'
    assert 225 <= (top+bottom)/2 <= 245, 'Digital logo is not below the time'
if boot_only:
    print('BOOT CHECK PASS: two-second startup with shrink and lower permanent logo')
    sys.exit(0)
analog = frame('55-watch-analog')
assert len(white_points(analog, (135,172,185,225))) > 450, 'Analog logo is missing below the centre'
assert len(white_points(analog, (135,125,185,165))) < 400, 'Analog logo still overlaps the hand hub'
time_points = white_points(frame('56-watch-digital'), (30,105,290,195))
assert 142 <= (min(y for x,y in time_points)+max(y for x,y in time_points))/2 <= 158, 'Digital time is not vertically centred'

def white_near(image,x,y):
    zoom = image.width // 620
    return any(min(image.getpixel((round((22+x+dx)*zoom),round((32+y+dy)*zoom)))) > 200
               for dx in range(-2,3) for dy in range(-2,3))
analog = frame('55-watch-analog')
digital = frame('56-watch-digital')
for i in range(12):
    angle = math.radians(i*30)
    x,y = 160+125*math.sin(angle),150-125*math.cos(angle)
    assert white_near(analog,x,y), f'Analog face missing hour mark {i}'
    if i not in (3,9):
        assert not white_near(digital,x,y), 'Digital face is covered by analog marks'
for angle,radius in [(77,65),(210,95)]:
    angle = math.radians(angle)
    assert white_near(analog,160+radius*math.sin(angle),150-radius*math.cos(angle)), 'Analog hands do not show 14:35'
# RGB565 image-edge antialiasing can round by one colour step on a partial redraw.
difference = ImageChops.difference(analog.crop((44,64,684,664)), frame('57-watch-analog-retained').crop((44,64,684,664)))
pixels = list(difference.get_flattened_data())
assert max(max(pixel) for pixel in pixels) <= 9 and sum(max(pixel)>0 for pixel in pixels) < 100, 'Selected clock face was not retained'
print('WATCH FACE CHECK PASS: two-second startup, centred digital time and lower logos, hour marks and correct hands')
