"""Rasterise the original Montserrat TTF at the display's actual pixel sizes."""
from pathlib import Path
import os
from PIL import ImageFont

root = Path(__file__).resolve().parents[1]
sdk = Path(os.environ.get("DIVE_NCS_ROOT", str(Path.home() / "ncs/v3.4.1")))
for size, weight in ((60, "Medium"), (84, "Medium"), (28, "Bold")):
    ttf = sdk / ("modules/lib/gui/lvgl/tests/src/test_files/fonts/Montserrat-Bold.ttf" if weight == "Bold" else "modules/lib/gui/lvgl/scripts/built_in_font/Montserrat-Medium.ttf")
    suffix = f"bold_{size}" if weight == "Bold" else str(size)
    font = ImageFont.truetype(str(ttf), size)
    bitmap = bytearray()
    descriptors = ['    {0},']
    for code in range(43, 59):  # punctuation, digits, colon
        character = chr(code)
        mask, (left, top) = font.getmask2(character, mode='L', anchor='ls')
        width, height = mask.size
        alpha = [(v + 8) // 17 for v in bytes(mask)]
        start = len(bitmap)
        for i in range(0, len(alpha), 2):
            bitmap.append((alpha[i] << 4) | (alpha[i+1] if i+1 < len(alpha) else 0))
        descriptors.append('    { .bitmap_index = %d, .adv_w = %d, .box_w = %d, .box_h = %d, .ofs_x = %d, .ofs_y = %d },' %
                           (start, round(font.getlength(character)*16), width, height, left, -top-height))
    rows = ['    '+', '.join(f'0x{b:02x}' for b in bitmap[i:i+16])+',' for i in range(0, len(bitmap), 16)]
    text = f'/* Montserrat {weight}, rasterised at native size; no runtime scaling. */\n#include <lvgl.h>\n'
    text += 'static const uint8_t bitmap[] = {\n'+'\n'.join(rows)+'\n};\n'
    text += 'static const lv_font_fmt_txt_glyph_dsc_t glyphs[] = {\n'+'\n'.join(descriptors)+'\n};\n'
    text += 'static const lv_font_fmt_txt_cmap_t cmaps[] = {{ .range_start = 43, .range_length = 16, .glyph_id_start = 1, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY }};\n'
    text += 'static const lv_font_fmt_txt_dsc_t descriptor = { .glyph_bitmap = bitmap, .glyph_dsc = glyphs, .cmaps = cmaps, .cmap_num = 1, .bpp = 4, .bitmap_format = 0 };\n'
    text += f'const lv_font_t watch_montserrat_{suffix} = {{ .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt, .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt, .line_height = {round(size*52/48)}, .base_line = {round(size*9/48)}, .dsc = &descriptor }};\n'
    (root/'src/assets'/f'montserrat_{suffix}.c').write_text(text)
