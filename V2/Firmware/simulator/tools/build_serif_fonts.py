from pathlib import Path
from PIL import ImageFont
root=Path(__file__).resolve().parents[1]/'src/assets'
for size in (14,18):
 font=ImageFont.truetype('/usr/share/fonts/liberation-serif-fonts/LiberationSerif-Regular.ttf',size)
 bitmap=bytearray();ds=['    {0},']
 for code in range(32,91):
  mask,(left,top)=font.getmask2(chr(code),mode='L',anchor='ls');w,h=mask.size
  alpha=[(v+8)//17 for v in bytes(mask)];start=len(bitmap)
  for i in range(0,len(alpha),2):bitmap.append((alpha[i]<<4)|(alpha[i+1] if i+1<len(alpha) else 0))
  ds.append('    { .bitmap_index=%d, .adv_w=%d, .box_w=%d, .box_h=%d, .ofs_x=%d, .ofs_y=%d },'%(start,round(font.getlength(chr(code))*16),w,h,left,-top-h))
 rows=['    '+','.join(f'0x{b:02x}' for b in bitmap[i:i+16])+',' for i in range(0,len(bitmap),16)]
 s='/* Liberation Serif (SIL Open Font License 1.1), native %d px ASCII subset. */\n#include <lvgl.h>\n'%size
 s+='static const uint8_t bitmap[]={\n'+'\n'.join(rows)+'\n};\nstatic const lv_font_fmt_txt_glyph_dsc_t glyphs[]={\n'+'\n'.join(ds)+'\n};\n'
 s+='static const lv_font_fmt_txt_cmap_t cmaps[]={{.range_start=32,.range_length=59,.glyph_id_start=1,.type=LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY}};\n'
 s+='static const lv_font_fmt_txt_dsc_t descriptor={.glyph_bitmap=bitmap,.glyph_dsc=glyphs,.cmaps=cmaps,.cmap_num=1,.bpp=4};\n'
 s+='const lv_font_t watch_serif_%d={.get_glyph_dsc=lv_font_get_glyph_dsc_fmt_txt,.get_glyph_bitmap=lv_font_get_bitmap_fmt_txt,.line_height=%d,.base_line=%d,.dsc=&descriptor};\n'%(size,size+3,3)
 (root/f'serif_{size}.c').write_text(s)
