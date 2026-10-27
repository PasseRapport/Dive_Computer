#include "light_editor.h"
lv_obj_t *light_editor_create(lv_obj_t *parent,const char *title,int maximum)
{
    lv_obj_t *view=lv_obj_create(parent);lv_obj_remove_style_all(view);lv_obj_set_size(view,320,300);
    lv_obj_set_style_bg_color(view,lv_color_black(),0);lv_obj_set_style_bg_opa(view,LV_OPA_COVER,0);
    lv_obj_remove_flag(view,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *heading=lv_label_create(view);lv_label_set_text(heading,title);
    lv_obj_set_style_text_font(heading,&lv_font_montserrat_18,0);lv_obj_set_style_text_color(heading,lv_color_white(),0);
    lv_obj_align(heading,LV_ALIGN_TOP_MID,0,51);
    lv_obj_t *bar=lv_bar_create(view);lv_obj_set_size(bar,180,18);lv_obj_set_pos(bar,70,135);
    lv_bar_set_range(bar,0,maximum);lv_obj_set_style_bg_color(bar,lv_color_hex(0x00bcbc),LV_PART_INDICATOR);
    lv_obj_t *hint=lv_label_create(view);lv_label_set_text(hint,"Tap to confirm");
    lv_obj_set_style_text_font(hint,&lv_font_montserrat_14,0);lv_obj_set_style_text_color(hint,lv_color_white(),0);
    lv_obj_align(hint,LV_ALIGN_TOP_MID,0,220);return view;
}
void light_editor_set_value(lv_obj_t *editor,int value){lv_bar_set_value(lv_obj_get_child(editor,1),value,LV_ANIM_OFF);}
