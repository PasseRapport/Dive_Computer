#include "apnea_settings.h"
#include "selection.h"
#include "sim_host.h"
LV_IMAGE_DECLARE(icon_drop);
LV_IMAGE_DECLARE(icon_wave);
static lv_obj_t *view,*heading,*pill,*rows[APNEA_BOTTOM_COUNT+1],*icons[2],*help,*choices,*anchor,*choice_pill;
static bool salt_water,underwater,engaged;
static enum apnea_bottom_field bottom;
static int entry,pending,editor;
static const char *const fields[]={"TEMPERATURE","MAX DEPTH","UNDERWATER TIME","CLOCK","DIVES"};
static void row_y(void *o,int32_t y){lv_obj_set_y(o,y);}
static void refresh(bool animate)
{
 lv_label_set_text(heading,editor==1?"WATER TYPE":editor==2?"BOTTOM FIELD":engaged?"SETTINGS":"OPTIONS");
 lv_obj_set_style_text_font(heading,engaged?&lv_font_montserrat_24:&lv_font_montserrat_18,0);
 lv_obj_align(heading,LV_ALIGN_TOP_MID,0,engaged?43:103);
 lv_label_set_text(help,engaged?"Double tap to return":"Tap to enter");
 lv_obj_set_style_text_color(help,engaged?lv_color_white():lv_color_hex(0xaaaaaa),0);
 lv_obj_align(help,LV_ALIGN_TOP_MID,0,engaged?257:176);
 int count=editor==2?APNEA_BOTTOM_COUNT+1:3,selected=editor?pending:entry;
 if(!engaged){
  for(int i=0;i<APNEA_BOTTOM_COUNT+1;i++)lv_obj_add_flag(rows[i],LV_OBJ_FLAG_HIDDEN);
  for(int i=0;i<2;i++)lv_obj_add_flag(icons[i],LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(choices,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(pill,LV_OBJ_FLAG_HIDDEN);
  return;
 }
 for(int i=0;i<APNEA_BOTTOM_COUNT+1;i++){
  if(i>=count){lv_obj_add_flag(rows[i],LV_OBJ_FLAG_HIDDEN);continue;}
  lv_obj_remove_flag(rows[i],LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text(rows[i],i==count-1?"BACK":editor==1?(i?"MARINE":"FRESH"):editor==2?fields[i]:(i?"BOTTOM FIELD":"WATER TYPE"));
  lv_obj_set_style_text_font(rows[i],&lv_font_montserrat_22,0);
  lv_obj_set_parent(rows[i],editor==1?view:choices);
  lv_anim_delete(rows[i],row_y);
  if(editor!=1){
   lv_obj_set_align(rows[i],LV_ALIGN_TOP_MID);lv_obj_set_x(rows[i],0);
   int target=64+52*(i-selected);
   if(animate){lv_anim_t a;lv_anim_init(&a);lv_anim_set_var(&a,rows[i]);lv_anim_set_values(&a,lv_obj_get_y(rows[i]),target);lv_anim_set_duration(&a,250);lv_anim_set_path_cb(&a,lv_anim_path_ease_in_out);lv_anim_set_exec_cb(&a,row_y);lv_anim_start(&a);}
   else lv_obj_set_y(rows[i],target);
  }
  if(editor==1){lv_obj_set_align(rows[i],LV_ALIGN_TOP_LEFT);lv_obj_set_pos(rows[i],130,100+56*i);}
  selection_row_color(rows[i],editor==1&&i<2?icons[i]:NULL,i==selected);
  lv_obj_set_style_text_opa(rows[i],underwater&&i!=count-1?LV_OPA_50:LV_OPA_COVER,0);
 }
 for(int i=0;i<2;i++){if(editor==1)lv_obj_remove_flag(icons[i],LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(icons[i],LV_OBJ_FLAG_HIDDEN);}
 if(editor==1){lv_obj_add_flag(choices,LV_OBJ_FLAG_HIDDEN);lv_obj_remove_flag(pill,LV_OBJ_FLAG_HIDDEN);selection_fit(pill,rows[selected],selected<2?icons[selected]:NULL,animate);}
 else {lv_obj_remove_flag(choices,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(pill,LV_OBJ_FLAG_HIDDEN);lv_label_set_text(anchor,lv_label_get_text(rows[selected]));lv_obj_align(anchor,LV_ALIGN_TOP_MID,0,64);selection_fit(choice_pill,anchor,NULL,animate);}
}
static lv_obj_t *label(const char *s,int y,const lv_font_t *font){lv_obj_t *o=lv_label_create(view);lv_label_set_text(o,s);lv_obj_set_style_text_font(o,font,0);lv_obj_set_style_text_color(o,lv_color_white(),0);lv_obj_align(o,LV_ALIGN_TOP_MID,0,y);return o;}
lv_obj_t *apnea_settings_create(lv_obj_t *parent)
{
 view=lv_obj_create(parent);lv_obj_remove_style_all(view);lv_obj_set_size(view,320,300);lv_obj_remove_flag(view,LV_OBJ_FLAG_SCROLLABLE);
 heading=label("SETTINGS",43,&lv_font_montserrat_24);
 pill=lv_obj_create(view);lv_obj_remove_style_all(pill);lv_obj_set_style_bg_color(pill,lv_color_hex(0x008b8b),0);lv_obj_set_style_bg_opa(pill,LV_OPA_COVER,0);
 choices=lv_obj_create(view);lv_obj_remove_style_all(choices);lv_obj_set_size(choices,320,155);lv_obj_set_pos(choices,0,73);lv_obj_remove_flag(choices,LV_OBJ_FLAG_SCROLLABLE);
 choice_pill=lv_obj_create(choices);lv_obj_remove_style_all(choice_pill);lv_obj_set_style_bg_color(choice_pill,lv_color_hex(0x008b8b),0);lv_obj_set_style_bg_opa(choice_pill,LV_OPA_COVER,0);
 anchor=lv_label_create(choices);lv_obj_set_style_text_font(anchor,&lv_font_montserrat_22,0);lv_obj_set_style_opa(anchor,LV_OPA_TRANSP,0);
 for(int i=0;i<APNEA_BOTTOM_COUNT+1;i++)rows[i]=label("",100,&lv_font_montserrat_22);
 for(int i=0;i<2;i++){icons[i]=lv_image_create(view);lv_image_set_src(icons[i],i?&icon_wave:&icon_drop);lv_obj_set_pos(icons[i],76,90+56*i);lv_obj_set_style_image_recolor_opa(icons[i],LV_OPA_COVER,0);}
 help=label("Tap to edit",257,&lv_font_montserrat_14);refresh(false);return view;
}
void apnea_settings_update(bool active){if(underwater!=active){underwater=active;refresh(false);}}
bool apnea_settings_input(enum watch_input input,int steps)
{
 if(!engaged){if(input!=WATCH_SINGLE_TAP)return false;engaged=true;refresh(false);return true;}
 if(input==WATCH_CROWN){int *selected=editor?&pending:&entry;*selected=LV_CLAMP(0,(int64_t)*selected+steps,editor==2?APNEA_BOTTOM_COUNT:2);refresh(true);}
 else if(input==WATCH_DOUBLE_TAP){if(editor)editor=0;else engaged=false;refresh(false);}
 else if(input==WATCH_SINGLE_TAP){
  bool go_back=editor?pending==(editor==2?APNEA_BOTTOM_COUNT:2):entry==2;
  if(go_back){if(editor)editor=0;else {engaged=false;entry=0;}}
  else if(underwater)return true;
  else if(editor){
   bool changed=editor==1?salt_water!=(bool)pending:bottom!=(enum apnea_bottom_field)pending;
   if(editor==1)salt_water=pending;else bottom=pending;
   if(changed)(void)sim_host_apnea_preferences_save(salt_water,bottom);
   editor=0;
  }
  else {editor=entry+1;pending=entry?bottom:salt_water;}
  refresh(false);
 }
 return true;
}
void apnea_settings_leave(void){engaged=false;editor=0;entry=0;refresh(false);}
bool apnea_settings_salt_water(void){return salt_water;}
enum apnea_bottom_field apnea_settings_bottom_field(void){return bottom;}
void apnea_settings_restore(bool salt,enum apnea_bottom_field field){salt_water=salt;bottom=field>=0&&field<APNEA_BOTTOM_COUNT?field:APNEA_TEMPERATURE;}

bool apnea_settings_selection_within_page(void)
{
 if(!engaged)return true;
 lv_obj_update_layout(view);lv_area_t box,page;
 lv_obj_get_coords(editor==1?pill:choice_pill,&box);lv_obj_get_coords(view,&page);
 return box.x1>=page.x1&&box.x2<=page.x2;
}
