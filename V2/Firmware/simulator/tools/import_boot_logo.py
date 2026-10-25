#!/usr/bin/env python3
"""Encode the supplied logo as a native LVGL RGB565 boot asset, keeping its aspect ratio."""
from pathlib import Path
from PIL import Image
root = Path(__file__).resolve().parents[1]
image = Image.open(root / 'src/assets/logo/poulpe-black.png').convert('RGB')
image.thumbnail((200, 210), Image.Resampling.LANCZOS)
payload = []
for r,g,b in image.get_flattened_data():
    pixel = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
    payload.extend((pixel & 255, pixel >> 8))
lines = [', '.join(f'0x{v:02x}' for v in payload[i:i+24]) for i in range(0,len(payload),24)]
w,h = image.size
source = '#include <lvgl.h>\n/* Supplied project logo, encoded by tools/import_boot_logo.py. */\n'
source += 'static const uint8_t project_logo_data[] = {\n    '+',\n    '.join(lines)+'\n};\n'
source += f'''const lv_image_dsc_t project_logo = {{
    .header = {{ .magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RGB565,
                .w = {w}, .h = {h}, .stride = {w*2} }},
    .data_size = sizeof(project_logo_data), .data = project_logo_data,
}};
'''
(root/'src/assets/project_logo.c').write_text(source)
print(f'Logo encoded at {w} x {h}, aspect ratio preserved')
