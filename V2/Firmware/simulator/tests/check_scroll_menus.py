#!/usr/bin/env python3
"""Native screenshots: fixed-focus submenus and moving main-menu selection."""
from pathlib import Path
from PIL import Image
import sys
root=Path(sys.argv[1])
def focus(name):
 im=Image.open(root/(name+'.bmp')).convert('RGB');s=im.width//620
 im=im.crop((22*s,32*s,342*s,332*s)).resize((320,300),Image.Resampling.NEAREST)
 pts=[(x,y) for y in range(300) for x in range(320) if (lambda r,g,b:r<30 and g>90 and b>90 and abs(g-b)<12)(*im.getpixel((x,y)))]
 assert len(pts)>500,f'{name}: selected capsule missing'
 return (min(y for x,y in pts)+max(y for x,y in pts))/2
for prefix,names in {
 'options':['first','watch','back'],
 'watch':['type','face','back'],
 'bluetooth':['first','data','back','x2','back-x2'],
 'haptics':['first','pulse','back'],
 'backlight':['first','brightness','back'],
 'dive':['scuba','free','back'],
 'scuba-water':['fresh','marine','back'],
 'apnea-water':['fresh','marine','back'],
 'apnea-field':['first','next','back'],
 'apnea-settings':['first','field','back'],
 'scuba-options':['first','next']
}.items():
 centers=[focus(prefix+'-'+name) for name in names]
 assert max(centers)-min(centers)<=2,(prefix,centers,'Selection moves instead of menu')
 assert all(abs(y-150)<=3 for y in centers),(prefix,centers,'Selection not centered')
 assert focus('main-options')-focus('main-watch')>160,'Main menu no longer has its moving capsule'
print('MENU IMAGES PASS: eleven menu groups keep the focus centered, including icons and x2; main menu keeps its moving selection')
