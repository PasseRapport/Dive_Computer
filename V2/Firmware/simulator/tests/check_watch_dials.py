#!/usr/bin/env python3
"""Validate captured native watch dials at both simulator scales."""
import math
import sys
from pathlib import Path
from PIL import Image, ImageChops

root=Path(sys.argv[1])
def frame(name,scale):
    image=Image.open(root/f'{name}-x{scale}.bmp').convert('RGB')
    return image.crop((22*scale,32*scale,342*scale,332*scale)).resize((320,300),Image.Resampling.NEAREST)
images={name:frame(name,1) for name in ('minimaliste','dauphine','automatique')}
for name,image in images.items():
    assert ImageChops.difference(image,frame(name,2)).getbbox() is None,f'{name}: x1/x2 differ'
for name in ('minimaliste','dauphine'):
    assert max(images[name].getpixel((160,150)))<20,f'{name}: unwanted central point'
for minute in range(60):
    angle=minute*math.pi/30
    x=round(160+140*math.sin(angle));y=round(150-140*math.cos(angle))
    patch=images['dauphine'].crop((x-2,y-2,x+3,y+3))
    # One-pixel diagonal strokes have anti-aliased gray cores.
    assert any(min(p)>=128 for p in patch.get_flattened_data()),f'Minute tick {minute} missing'
auto=images['automatique']
r,g,b=auto.getpixel((160,200))
assert b>g>r and b-r>20,'Automatic dial blue background missing'
assert not any(max(p)<10 for p in auto.crop((140,83,180,120)).get_flattened_data()),'Logo has a black rectangular background'
for a,b in (('minimaliste','dauphine'),('dauphine','automatique')):
    assert ImageChops.difference(images[a],images[b]).getbbox(),f'{a}/{b} unexpectedly identical'
print('WATCH DIAL IMAGE PASS: three distinct dials, sixty minute ticks, central-point rules, transparent logo and exact x1/x2 parity')
