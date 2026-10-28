"""Verify visible Statistics geometry through native interaction captures."""
from pathlib import Path
import sys
import math
from PIL import Image
root=Path(sys.argv[1])
def frame(name):
    image=Image.open(root/(name+'.bmp')).convert('RGB');zoom=image.width//620
    native=Image.new('RGB',(320,300))
    native.putdata([image.getpixel(((22+x)*zoom,(32+y)*zoom)) for y in range(300) for x in range(320)])
    return native
def points(im,predicate,box):
    x1,y1,x2,y2=box
    return {(x,y) for y in range(y1,y2) for x in range(x1,x2) if predicate(im.getpixel((x,y)))}
white=lambda p:min(p)>220
orange=lambda p:p[0]>220 and 130<p[1]<190 and p[2]<90
cyan=lambda p:p[0]<90 and p[1]>180 and p[2]>180
if '--baseline' in sys.argv:
    old=frame('apnea-14-statistics-no-ring')
    assert len(points(old,orange,(100,118,220,119)))>110,'Statistics lacks common orange separator'
    raise AssertionError('Old Statistics unexpectedly meets new geometry')
for name in ['stats-01-fresh','stats-02-live','stats-03-recovery','stats-04-second-shorter',
             'stats-05-live-record-x1','stats-06-live-record-x2','stats-07-long-session','stats-08-reentry']:
    im=frame(name)
    for y in (118,215):
        assert len(points(im,orange,(100,y,220,y+1)))>110,f'{name}: orange separator missing'
    assert not points(im,cyan,(90,118,230,225)),f'{name}: Main separators leaked'
    assert len(points(im,white,(140,12,180,34)))>15,f'{name}: battery missing'
    assert not points(im,white,(151,145,170,203)),f'{name}: Main direction indicator leaked'
    for degrees in range(0,360,5):
        a=math.radians(degrees);p=im.getpixel((round(160+144*math.cos(a)),round(150+144*math.sin(a))))
        assert not (p[0]>180 and p[1]<80 and p[2]<80) and not (p[1]>180 and p[0]<80 and p[2]<80),f'{name}: recovery ring leaked'
    # Meaningful portions of the shared large/central/footer fields contain text.
    for box in [(65,47,255,107),(47,151,146,190),(173,151,267,190),(65,239,255,279)]:
        assert len(points(im,white,box))>15,f'{name}: missing field {box}'
returned=frame('stats-main-return')
assert any((lambda p:p[0]>180 and p[1]<80 and p[2]<80)(returned.getpixel((round(160+144*math.cos(math.radians(d))),round(150+144*math.sin(math.radians(d)))))) for d in range(360)), 'Returning to Main failed to restore recovery ring'
main=frame('apnea-17-long-x1')
for name in ['stats-02-live','stats-05-live-record-x1','stats-06-live-record-x2']:
    im=frame(name)
    assert len(points(im,white,(157,89,164,101)))>8,f'{name}: depth decimal anchor moved'
    for y in (118,215):
        # Compare actual separator spans including their curved ends.
        actual=points(im,orange,(30,y,290,y+10));reference=points(main,cyan,(30,y,290,y+10))
        # RGB565 quantization differs between orange/cyan antialiased edge pixels.
        for left,right in ((actual,reference),(reference,actual)):
            assert all(any((x+dx,y+dy) in right for dx in (-1,0,1) for dy in (-1,0,1)) for x,y in left),f'{name}: separator geometry differs from Main'
print('APNEA STATS RENDER PASS: shared geometry/orange rails, battery, four fields, no direction, decimal anchor, x1/x2 and long session')
