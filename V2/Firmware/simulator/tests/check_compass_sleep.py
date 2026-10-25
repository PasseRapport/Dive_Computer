"""Rendered checks for the rotating compass and logo-to-black sleep transition."""
from pathlib import Path
import math
import sys
from PIL import Image, ImageStat
root = Path(sys.argv[1])

def frame(name):
    return Image.open(root/(name+'.bmp')).convert('RGB')

for name,heading in [('62-compass',0),('65-compass-east',90),('66-compass-south',180),('67-compass-west',270),('68-compass-north',0),('69-compass-wrap',355)]:
    image = frame(name)
    zoom = image.width//620
    angle = math.radians(-heading)
    cx,cy = 160+110*math.sin(angle),150-110*math.cos(angle)
    north_pixels=0
    for x in range(round(cx)-14,round(cx)+15):
        for y in range(round(cy)-14,round(cy)+15):
            r,g,b=image.getpixel(((22+x)*zoom,(32+y)*zoom))
            north_pixels += r < 70 and g > 130 and b > 130
    assert north_pixels > 25, f'{name}: North does not rotate to match heading {heading}'
    # The rose must reach the perimeter at every cardinal direction.
    for angle_degrees in (0, 90, 180, 270):
        rad=math.radians(angle_degrees)
        x,y=160+144*math.sin(rad),150-144*math.cos(rad)
        pixels=[image.getpixel(((22+round(x)+dx)*zoom,(32+round(y)+dy)*zoom))
                for dx in range(-2,3) for dy in range(-2,3)]
        assert any(max(rgb)>100 for rgb in pixels), f'{name}: compass does not fill the screen'
    print(f'{name}: rotating North marker verified at {heading} degrees')

def brightness(name):
    image=frame(name);zoom=image.width//620
    return sum(ImageStat.Stat(image.crop((82*zoom,78*zoom,282*zoom,286*zoom))).mean)/3
start,logo,fade=map(brightness,['70-sleep-start','71-sleep-logo','72-sleep-fade'])
assert logo > 30, 'Sleep logo is missing'
assert 0.1*logo < start < 0.95*logo, 'Sleep transition does not fade onto the logo'
assert 0.15*logo < fade < 0.8*logo, 'Sleep logo does not fade into black'
image=frame('15-display-off');zoom=image.width//620
for x in range(30,290):
    for y in range(20,280):
        if (x-160)**2+(y-150)**2 < 140**2:
            assert max(image.getpixel(((22+x)*zoom,(32+y)*zoom))) == 0, 'Display is not fully black after sleep'
print('COMPASS/SLEEP CHECK PASS: full heading rotation, 360 wrap, sleep logo fade and black endpoint')
