"""Observe restored choices and cross-page agreement in separate-process captures."""
from pathlib import Path
import sys
from PIL import Image
root=Path(sys.argv[1])
def frame(directory,name):
    im=Image.open(directory/(name+'.bmp')).convert('RGB')
    return im.crop((22,32,342,332))
def white(im,box):
    x1,y1,x2,y2=box
    return {(x,y) for y in range(y1,y2) for x in range(x1,x2) if min(im.getpixel((x,y)))>220}
loads=list(root.glob('choice-*-load'))
assert len(loads)==6,'Expected six independent restored UI captures'
for directory in loads:
    choice=int(directory.name.split('-')[-2]);water=frame(directory,'restored-water')
    teal=[(x,y) for y in range(80,215) for x in range(20,300)
          if (lambda p:p[0]<70 and p[1]>80 and p[2]>80)(water.getpixel((x,y)))]
    assert len(teal)>1000,'Restored water has no selected capsule'
    centre=(min(y for x,y in teal)+max(y for x,y in teal))/2
    assert abs(centre-(112+56*(choice//3)))<8,'Restored water highlights the wrong visible row'
    main=frame(directory,'live-main');stats=frame(directory,'live-statistics');returned=frame(directory,'returned-main')
    assert white(main,(55,47,265,107))==white(stats,(55,47,265,107)),'Main and Statistics disagree on live depth rendering'
    assert white(main,(65,239,255,279))==white(returned,(65,239,255,279)),'Returning to Main changed the selected field/value'
    assert len(white(frame(directory,'restored-main'),(65,239,255,279)))>20,'Restored Main footer missing'
    print(f'VISIBLE RESTART PASS: water={choice//3}, bottom={choice%3}, selected row and Main/Statistics/Main records')
print('APNEA PERSISTENCE RENDER PASS: six restored choices, active record agreement, selected field retained')
