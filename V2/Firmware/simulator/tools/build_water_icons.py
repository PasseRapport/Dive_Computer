#!/usr/bin/env python3
"""Rasterize native 44 px water icons from Bezier paths; no old bitmap scaling."""
from pathlib import Path
from PIL import Image, ImageDraw
ROOT = Path(__file__).resolve().parents[1]
SIZE, SCALE = 44, 4

def path(start, segments):
    points = [start]
    for kind, *coords in segments:
        if kind == 'L':
            points.append(tuple(coords)); continue
        x0, y0 = points[-1]
        x1, y1, x2, y2, x3, y3 = coords
        for i in range(1, 65):
            t = i / 64; u = 1-t
            points.append((u**3*x0+3*u*u*t*x1+3*u*t*t*x2+t**3*x3,
                           u**3*y0+3*u*u*t*y1+3*u*t*t*y2+t**3*y3))
    return [(round(x*SCALE), round(y*SCALE)) for x,y in points]

def render(name, shapes):
    mask = Image.new('L', (SIZE*SCALE, SIZE*SCALE))
    draw = ImageDraw.Draw(mask)
    for value, start, segments in shapes:
        draw.polygon(path(start, segments), fill=value)
    mask = mask.resize((SIZE,SIZE), Image.Resampling.LANCZOS)
    # White RGB + alpha allows LVGL recolouring without coloured fringes.
    payload = [channel for alpha in mask.tobytes() for channel in (255,255,255,alpha)]
    lines = [', '.join(f'0x{v:02x}' for v in payload[i:i+24]) for i in range(0,len(payload),24)]
    source = '#include <lvgl.h>\n/* Native 44 px icon; generated from Bezier paths in tools/build_water_icons.py. */\n'
    source += f'static const uint8_t {name}_data[] = {{\n    '+',\n    '.join(lines)+'\n};\n'
    source += f'''const lv_image_dsc_t {name} = {{
    .header = {{ .magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_ARGB8888,
                .w = 44, .h = 44, .stride = 176 }},
    .data_size = sizeof({name}_data), .data = {name}_data,
}};
'''
    (ROOT/'src/assets'/f'{name}.c').write_text(source)
    image = Image.new('RGBA',(SIZE,SIZE),'white'); image.putalpha(mask)
    image.save('/tmp/'+name+'-native.png')

render('icon_drop', [
    (255, (22,2), [('C',19,9,7,19,7,29),('C',7,47,37,47,37,29),('C',37,19,25,9,22,2)]),
    (0, (20,12), [('C',16,18,11,24,11,30),('C',11,33,12,35,14,36),('C',12,26,18,20,22,15),('L',20,12)])
])
render('icon_wave', [
    (255, (3,31), [('C',10,28,9,18,18,11),('C',26,4,37,8,39,17),('C',33,11,26,14,26,21),('C',26,29,34,30,41,29),('L',41,33),('C',35,36,30,31,25,33),('C',18,37,15,31,9,34),('L',3,34)]),
    (255, (3,38), [('C',10,34,14,41,21,37),('C',28,33,32,41,41,36),('L',41,39),('C',32,44,28,36,21,40),('C',14,44,10,37,3,41)])
])
