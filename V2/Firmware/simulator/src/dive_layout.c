#include "dive_layout.h"
#include "native_fonts.h"
#include <stdio.h>
void dive_layout_depth(lv_obj_t *depth,lv_obj_t *unit,uint16_t dm)
{
    char value[20],whole[16];snprintf(value,sizeof(value),"%u.%u",dm/10,dm%10);
    snprintf(whole,sizeof(whole),"%u",dm/10);
    lv_label_set_text(depth,value);lv_obj_update_layout(depth);lv_obj_update_layout(unit);
    lv_point_t prefix;
    lv_text_get_size(&prefix,whole,&watch_montserrat_60,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
    lv_font_glyph_dsc_t dot;
    lv_font_get_glyph_dsc(&watch_montserrat_60,&dot,'.','0'+dm%10);
    int x=160-prefix.x-dot.ofs_x-dot.box_w/2+1;
    lv_obj_set_pos(depth,x,47);
    const lv_font_t *small=&lv_font_montserrat_18;
    lv_obj_set_pos(unit,x+lv_obj_get_width(depth)+5,
        47+watch_montserrat_60.line_height-watch_montserrat_60.base_line-small->line_height+small->base_line);
}
static void draw_rail(lv_event_t *event)
{
    lv_area_t area;lv_obj_get_coords(lv_event_get_target_obj(event),&area);
    /* At the lower latitude, move both ends inwards to preserve the same
     * radial gap to the round display edge as the upper separator. */
    int inset=(int)(uintptr_t)lv_event_get_user_data(event);
    lv_layer_t *layer=lv_event_get_layer(event);
    lv_draw_rect_dsc_t ink;lv_draw_rect_dsc_init(&ink);ink.bg_color=lv_obj_get_style_text_color(lv_event_get_target_obj(event),0);ink.bg_opa=LV_OPA_COVER;
    lv_area_t middle={area.x1+67+inset,area.y1,area.x1+252-inset,area.y1};lv_draw_rect(layer,&ink,&middle);
    for(int x=40;x<67;x++) {
        int fixed=x<=52?512+(52-x)*1536/12:(67-x)*512/15;
        int y=fixed/256,fraction=fixed%256;
        for(int side=0;side<2;side++) {
            int px=area.x1+(side?319-x-inset:x+inset);
            lv_area_t pixel={px,area.y1+y,px,area.y1+y};
            ink.bg_opa=255-fraction;lv_draw_rect(layer,&ink,&pixel);
            if(fraction) {pixel.y1++;pixel.y2++;ink.bg_opa=fraction;lv_draw_rect(layer,&ink,&pixel);}
        }
    }
}
void dive_layout_rails(lv_obj_t *parent,uint32_t color)
{
    for(int i=0;i<2;i++) {
        lv_obj_t *rail=lv_obj_create(parent);lv_obj_remove_style_all(rail);
        lv_obj_set_pos(rail,0,i?215:118);lv_obj_set_size(rail,320,10);
        lv_obj_set_style_text_color(rail,lv_color_hex(color),0);
        lv_obj_remove_flag(rail,LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(rail,draw_rail,LV_EVENT_DRAW_MAIN,(void *)(uintptr_t)(i?23:0));
    }
}
