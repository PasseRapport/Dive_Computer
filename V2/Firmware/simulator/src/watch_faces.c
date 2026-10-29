#include "watch_faces.h"
#include "native_fonts.h"
#include <stdio.h>
#include <string.h>
LV_IMAGE_DECLARE(project_logo_alpha);
LV_FONT_DECLARE(watch_serif_14);
LV_FONT_DECLARE(watch_serif_18);
struct face {
    enum watch_clock_style type;
    int dial;
    lv_obj_t *root,*legacy,*logo,*art;
    struct watch_data data;
};
static const char *const analog_names[]={"MINIMALISTE","DAUPHINE","AUTOMATIQUE","RADAR","BLEU POLAIRE","CLASSIQUE IVOIRE","SOLAIRE"};
static const char *const digital_names[]={"EMBLEME","TERMINAL","TOTEM","SPORT","SCEAU"};
static const char *const days[]={"SUN","MON","TUE","WED","THU","FRI","SAT"};
static const char *const months[]={"JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"};
int watch_face_count(enum watch_clock_style type){return type==CLOCK_ANALOG?DIAL_COUNT:DIGITAL_COUNT;}
const char *watch_face_name(enum watch_clock_style type,int dial)
{
    if(dial<0||dial>=watch_face_count(type))return "";
    return type==CLOCK_ANALOG?analog_names[dial]:digital_names[dial];
}
static void rect(lv_layer_t *l,int x,int y,int w,int h,unsigned color,int radius)
{
    lv_draw_rect_dsc_t d;lv_draw_rect_dsc_init(&d);d.bg_opa=255;d.bg_color=lv_color_hex(color);d.radius=radius;
    lv_area_t a={x,y,x+w-1,y+h-1};lv_draw_rect(l,&d,&a);
}
static void label(lv_layer_t *l,const char *value,const lv_font_t *font,unsigned color,int x,int y,bool centered)
{
    lv_point_t size;lv_text_get_size(&size,value,font,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
    if(centered)x-=size.x/2;
    lv_draw_label_dsc_t d;lv_draw_label_dsc_init(&d);d.text=value;d.text_local=1;d.font=font;d.color=lv_color_hex(color);d.opa=255;
    lv_area_t a={x,y,x+size.x-1,y+size.y-1};lv_draw_label(l,&d,&a);
}
static lv_point_precise_t point(const lv_area_t *a,int angle,int radius,int side)
{
    return (lv_point_precise_t){a->x1+160+((lv_trigo_sin(angle)*radius+lv_trigo_sin(angle+90)*side)>>LV_TRIGO_SHIFT),
        a->y1+150+((-lv_trigo_sin(angle+90)*radius+lv_trigo_sin(angle)*side)>>LV_TRIGO_SHIFT)};
}
static void stroke(lv_layer_t *l,const lv_area_t *a,int angle,int start,int end,int width,unsigned color)
{
    lv_draw_line_dsc_t d;lv_draw_line_dsc_init(&d);d.p1=point(a,angle,start,0);d.p2=point(a,angle,end,0);
    d.width=width;d.color=lv_color_hex(color);d.opa=255;lv_draw_line(l,&d);
}
static void edge(lv_layer_t *l,int x,int y,int xx,int yy,unsigned color)
{
    lv_draw_line_dsc_t d;lv_draw_line_dsc_init(&d);d.p1=(lv_point_precise_t){x,y};d.p2=(lv_point_precise_t){xx,yy};d.width=1;d.color=lv_color_hex(color);d.opa=255;lv_draw_line(l,&d);
}
static void triangle(lv_layer_t *l,const lv_area_t *a,int angle,const int (*pts)[2],int n,unsigned color)
{
    for(int i=1;i<n-1;i++) {
        lv_draw_triangle_dsc_t d;lv_draw_triangle_dsc_init(&d);d.p[0]=point(a,angle,pts[0][0],pts[0][1]);
        d.p[1]=point(a,angle,pts[i][0],pts[i][1]);d.p[2]=point(a,angle,pts[i+1][0],pts[i+1][1]);d.color=lv_color_hex(color);d.opa=255;lv_draw_triangle(l,&d);
    }
}
static void circle(lv_layer_t *l,const lv_area_t *a,int radius,unsigned color)
{
    lv_draw_arc_dsc_t d;lv_draw_arc_dsc_init(&d);d.center=(lv_point_t){a->x1+160,a->y1+150};d.radius=radius;
    d.start_angle=0;d.end_angle=360;d.width=1;d.color=lv_color_hex(color);d.opa=255;lv_draw_arc(l,&d);
}
static void radial_label(lv_layer_t *l,const lv_area_t *a,int angle,int radius,const char *s,const lv_font_t *font,unsigned color)
{
    lv_point_precise_t q=point(a,angle,radius,0);label(l,s,font,color,q.x,q.y-font->line_height/2,true);
}
static void dot_digit(lv_layer_t *l,int x,int y,int value)
{
    static const unsigned bits[10][7]={
        {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},{30,1,1,14,1,1,30},
        {2,6,10,18,31,2,2},{31,16,16,30,1,1,30},{14,16,16,30,17,17,14},
        {31,1,2,4,8,8,8},{14,17,17,14,17,17,14},{14,17,17,15,1,1,14}};
    for(int r=0;r<7;r++)for(int c=0;c<5;c++)rect(l,x+c*8,y+r*8,5,5,bits[value][r]&(1u<<(4-c))?0xb5d6bd:0x17261e,2);
}
static void lcd_digit(lv_layer_t *l,int x,int y,int value)
{
    static const unsigned mask[]={0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f};
    const int p[7][4]={{6,0,36,6},{42,6,6,28},{42,40,6,22},{6,62,36,6},{0,40,6,22},{0,6,6,28},{6,34,36,6}};
    for(int i=0;i<7;i++)rect(l,x+p[i][0],y+p[i][1],p[i][2],p[i][3],mask[value]&(1u<<i)?0x152119:0x9fac92,1);
}
static void roman(unsigned n,char *out,size_t size)
{
    static const unsigned value[]={100,90,50,40,10,9,5,4,1};
    static const char *const symbol[]={"C","XC","L","XL","X","IX","V","IV","I"};
    if(!n){snprintf(out,size,"N");return;}out[0]=0;
    for(unsigned i=0;i<9;i++)while(n>=value[i]){size_t used=strlen(out);snprintf(out+used,size-used,"%s",symbol[i]);n-=value[i];}
}
static void draw_analog(lv_layer_t *l,const lv_area_t *a,const struct face *f,bool foreground)
{
    bool radar=f->dial==DIAL_RADAR,ivory=f->dial==DIAL_IVORY,solar=f->dial==DIAL_SOLAR;
    unsigned ink=radar?0xb9ffce:ivory?0x252b2a:solar?0xc9b486:0x183e59;
    unsigned accent=radar?0x8ef9b3:ink;
    int x=a->x1,y=a->y1;char buf[40];
    if(!foreground) {
        for(int i=0;i<60;i++) {
            if(solar&&i%5)continue;
            stroke(l,a,i*6,i%5?(radar?140:138):(radar?133:130),143,i%5?1:2,radar&&i%5?0x1c4930:ink);
        }
        if(radar) {
            for(int r=45;r<=135;r+=45)circle(l,a,r,0x1c4930);
            for(int i=0;i<4;i++)stroke(l,a,i*90,15,132,1,0x1c4930);
            for(int i=0;i<12;i++) {
                lv_point_precise_t q=point(a,i*30,90,0);rect(l,q.x-1,q.y-1,3,3,0x1c4930,1);
                int lit=(f->data.battery_percent*12+50)/100;
                stroke(l,a,226+i*8,100,108,4,i<lit?accent:0x39443c);
            }
            rect(l,x+119,y+180,82,57,0x07170f,0);
            for(int i=0;i<2;i++)for(int j=0;j<2;j++) {
                int xx=x+119+i*82,yy=y+180+j*57;
                edge(l,xx,yy,xx+(i?-7:7),yy,accent);edge(l,xx,yy,xx,yy+(j?-7:7),accent);
            }
        }
        return;
    }
    for(int i=0;i<4;i++) {
        if(solar) {const char *r[]={"XII","III","VI","IX"};radial_label(l,a,i*90,120,r[i],&watch_serif_18,ink);}
        else {snprintf(buf,sizeof(buf),"%d",i?i*3:12);radial_label(l,a,i*90,121,buf,radar?&lv_font_montserrat_16:&watch_montserrat_bold_28,ink);}
    }
    const char *day=days[f->data.clock_weekday%7],*month=months[(f->data.clock_month?f->data.clock_month-1:0)%12];
    if(radar) {
        label(l,day,&lv_font_montserrat_10,accent,x+160,y+184,true);
        snprintf(buf,sizeof(buf),"%02u",f->data.clock_day);label(l,buf,&lv_font_montserrat_22,ink,x+160,y+196,true);
        label(l,month,&lv_font_montserrat_10,accent,x+160,y+222,true);
    } else {
        snprintf(buf,sizeof(buf),solar?"%s %02u %s":"%s %02u",day,f->data.clock_day,month);
        label(l,buf,solar?&watch_serif_14:&lv_font_montserrat_14,ink,x+(ivory?209:160),y+(ivory?143:207),!ivory);
        if(solar)roman(f->data.battery_percent,buf,sizeof(buf));else snprintf(buf,sizeof(buf),"%u%%",f->data.battery_percent);
        label(l,buf,solar?&watch_serif_14:&lv_font_montserrat_12,ink,x+160,y+241,true);
    }
    for(int minute=0;minute<2;minute++) {
        int angle=minute?f->data.clock_minute*6+f->data.clock_second/10:(f->data.clock_hour%12)*30+f->data.clock_minute/2;
        int tip=minute?(radar?119:115):84;
        if(radar||ivory)stroke(l,a,angle,-8,tip,minute?3:(radar?6:5),minute?ink:accent);
        else if(solar) {
            int shoulder=minute?51:40,w=minute?5:9;
            int left[][2]={{19,0},{shoulder,-w},{tip,0}},right[][2]={{19,0},{tip,0},{shoulder,w}};
            triangle(l,a,angle,left,3,ink);triangle(l,a,angle,right,3,0x776243);
        } else {
            int shoulder=minute?62:42,w=minute?5:8;
            int outer[][2]={{-6,-2},{shoulder,-w},{tip,0},{shoulder,w},{-6,2}};
            int inner[][2]={{12,-1},{shoulder,-w+3},{tip-12,0},{shoulder,w-3},{12,1}};
            triangle(l,a,angle,outer,5,ink);triangle(l,a,angle,inner,5,0xe5e6d7);
        }
    }
    if(!solar)rect(l,x+157,y+147,7,7,ink,4);
}
static void draw_digital(lv_layer_t *l,const lv_area_t *a,const struct face *f,bool foreground)
{
    int x=a->x1,y=a->y1,d=f->dial;unsigned ink=d==DIGITAL_SPORT?0x152119:d==DIGITAL_TERMINAL?0xb5d6bd:0xffffff;
    char buf[64];int digits[]={f->data.clock_hour/10,f->data.clock_hour%10,f->data.clock_minute/10,f->data.clock_minute%10};
    if(!foreground) {
        if(d==DIGITAL_SEAL)rect(l,x+144,y+83,1,137,0x52697b,0);
        if(d==DIGITAL_TERMINAL) {rect(l,x+47,y+101,226,1,0x3d5b48,0);rect(l,x+47,y+207,226,1,0x3d5b48,0);}
        return;
    }
    snprintf(buf,sizeof(buf),"%s %02u %s",days[f->data.clock_weekday%7],f->data.clock_day,months[(f->data.clock_month?f->data.clock_month-1:0)%12]);
    if(d==DIGITAL_SPORT) {
        label(l,days[f->data.clock_weekday%7],&lv_font_montserrat_16,ink,x+47,y+105,false);
        snprintf(buf,sizeof(buf),"%02u %s",f->data.clock_day,months[(f->data.clock_month?f->data.clock_month-1:0)%12]);
        label(l,buf,&lv_font_montserrat_16,ink,x+207,y+105,false);
        for(int i=0;i<4;i++)lcd_digit(l,x+40+(i<2?i*56:(i-2)*56+126),y+133,digits[i]);
        rect(l,x+154,y+155,4,4,ink,0);rect(l,x+154,y+177,4,4,ink,0);
        edge(l,x+47,y+218,x+273,y+218,0x53634b);
        label(l,"24H",&lv_font_montserrat_12,ink,x+65,y+239,false);
        /* Period-style battery outline and five discrete LCD segments. */
        edge(l,x+132,y+239,x+197,y+239,ink);edge(l,x+132,y+251,x+197,y+251,ink);
        edge(l,x+132,y+239,x+132,y+251,ink);edge(l,x+197,y+239,x+197,y+251,ink);rect(l,x+198,y+243,3,5,ink,0);
        int lit=(f->data.battery_percent*5+50)/100;
        for(int i=0;i<5;i++)rect(l,x+135+i*12,y+242,9,7,i<lit?ink:0x9fac92,0);
    } else if(d==DIGITAL_TERMINAL) {
        label(l,buf,&lv_font_montserrat_16,ink,x+160,y+72,true);
        for(int i=0;i<4;i++)dot_digit(l,x+49+(i<2?i*48:(i-2)*48+114),y+126,digits[i]);
        rect(l,x+145,y+142,5,5,ink,2);rect(l,x+145,y+166,5,5,ink,2);
        char bars[11];int lit=(f->data.battery_percent*10+50)/100;
        for(int i=0;i<10;i++)bars[i]=i<lit?'|':'.';
        bars[10]=0;
        snprintf(buf,sizeof(buf),"PWR [%s]",bars);label(l,buf,&lv_font_montserrat_14,ink,x+160,y+230,true);
    } else {
        label(l,buf,&lv_font_montserrat_16,d==DIGITAL_EMBLEM?0xc8e4ed:d==DIGITAL_TOTEM?0xa5b4be:0xb9c9d6,x+160,y+(d==DIGITAL_TOTEM?34:d==DIGITAL_EMBLEM?47:42),true);
        if(d==DIGITAL_SEAL) {
            snprintf(buf,sizeof(buf),"%02u",f->data.clock_hour);label(l,buf,&watch_montserrat_60,ink,x+170,y+83,false);
            snprintf(buf,sizeof(buf),"%02u",f->data.clock_minute);label(l,buf,&watch_montserrat_60,ink,x+170,y+151,false);
        } else {
            snprintf(buf,sizeof(buf),"%02u:%02u",f->data.clock_hour,f->data.clock_minute);
            label(l,buf,d==DIGITAL_EMBLEM?&watch_montserrat_84:&watch_montserrat_60,ink,x+160,y+(d==DIGITAL_EMBLEM?105:174),true);
        }
        snprintf(buf,sizeof(buf),"%u%%",f->data.battery_percent);label(l,buf,&lv_font_montserrat_14,0xa5bdc9,x+160,y+(d==DIGITAL_TOTEM?263:250),true);
    }
}
static void draw(lv_event_t *e)
{
    struct face *f=lv_event_get_user_data(e);lv_obj_t *o=lv_event_get_target_obj(e);lv_area_t a;lv_obj_get_coords(o,&a);
    bool foreground=o==f->art;
    if(f->type==CLOCK_ANALOG)draw_analog(lv_event_get_layer(e),&a,f,foreground);
    else draw_digital(lv_event_get_layer(e),&a,f,foreground);
}
static void release(lv_event_t *e){lv_free(lv_event_get_user_data(e));}
static lv_obj_t *drawing(struct face *f)
{
    lv_obj_t *o=lv_obj_create(f->root);lv_obj_remove_style_all(o);lv_obj_set_size(o,320,300);
    lv_obj_remove_flag(o,LV_OBJ_FLAG_SCROLLABLE);lv_obj_remove_flag(o,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(o,draw,LV_EVENT_DRAW_MAIN,f);return o;
}
lv_obj_t *watch_face_create(lv_obj_t *parent,enum watch_clock_style type,int dial)
{
    if(dial<0||dial>=watch_face_count(type))return NULL;
    struct face *f=lv_malloc_zeroed(sizeof(*f));LV_ASSERT_MALLOC(f);f->type=type;f->dial=dial;
    f->root=lv_obj_create(parent);lv_obj_remove_style_all(f->root);lv_obj_set_size(f->root,320,300);
    lv_obj_set_user_data(f->root,f);lv_obj_add_event_cb(f->root,release,LV_EVENT_DELETE,f);
    lv_obj_remove_flag(f->root,LV_OBJ_FLAG_SCROLLABLE);
    if(type==CLOCK_ANALOG&&dial<=DIAL_AUTOMATIC) {
        f->legacy=analog_clock_create(f->root);analog_clock_set_dial(f->legacy,dial);return f->root;
    }
    unsigned bg=type==CLOCK_ANALOG?(dial==DIAL_RADAR?0x07170f:dial==DIAL_POLAR?0xd4e5e9:dial==DIAL_IVORY?0xe9e6db:0x1b1a18):
        dial==DIGITAL_EMBLEM?0x0b2433:dial==DIGITAL_TERMINAL?0x0d1d13:dial==DIGITAL_SPORT?0xaab99b:0;
    lv_obj_set_style_bg_color(f->root,lv_color_hex(bg),0);lv_obj_set_style_bg_opa(f->root,255,0);
    drawing(f);
    int cx=160,cy=0,scale=0;unsigned color=0xffffff;
    if(type==CLOCK_ANALOG&&dial==DIAL_POLAR){cy=103;scale=110;color=0x244a60;}
    if(type==CLOCK_DIGITAL) {
        if(dial==DIGITAL_EMBLEM){cy=150;scale=216;color=0x3e7183;}
        if(dial==DIGITAL_TOTEM){cy=109;scale=110;color=0xf6f6f6;}
        if(dial==DIGITAL_SEAL){cx=83;cy=157;scale=120;color=0xecf5f7;}
    }
    if(scale) {
        f->logo=lv_image_create(f->root);lv_image_set_src(f->logo,&project_logo_alpha);lv_image_set_scale(f->logo,scale);lv_image_set_antialias(f->logo,true);
        lv_obj_set_pos(f->logo,cx-100,cy-209/2);lv_obj_set_style_image_recolor(f->logo,lv_color_hex(color),0);lv_obj_set_style_image_recolor_opa(f->logo,255,0);
    }
    f->art=drawing(f);return f->root;
}
void watch_face_update(lv_obj_t *view,const struct watch_data *data)
{
    struct face *f=lv_obj_get_user_data(view);
    /* UI input is bounded by the host, but keep rendering safe for adapters. */
    struct watch_data next=*data;next.battery_percent=LV_MIN(100,next.battery_percent);next.clock_hour%=24;next.clock_minute%=60;next.clock_second%=60;
    if(f->legacy)analog_clock_update(f->legacy,&next);
    else if(memcmp(&f->data,&next,sizeof(next))) {f->data=next;lv_obj_invalidate(view);}
}
lv_obj_t *watch_face_logo(lv_obj_t *view){struct face *f=lv_obj_get_user_data(view);return f->legacy?analog_clock_logo(f->legacy):f->logo;}
int watch_face_logo_x(lv_obj_t *view){struct face *f=lv_obj_get_user_data(view);return f->type==CLOCK_DIGITAL&&f->dial==DIGITAL_SEAL?83:160;}
int watch_face_logo_y(lv_obj_t *view)
{
    struct face *f=lv_obj_get_user_data(view);return f->legacy?analog_clock_logo_y(f->legacy):f->type==CLOCK_ANALOG?103:f->dial==DIGITAL_TOTEM?109:f->dial==DIGITAL_SEAL?157:150;
}
int watch_face_logo_scale(lv_obj_t *view)
{
    struct face *f=lv_obj_get_user_data(view);return f->legacy?analog_clock_logo_scale(f->legacy):f->type==CLOCK_ANALOG?110:f->dial==DIGITAL_TOTEM?110:f->dial==DIGITAL_SEAL?120:216;
}
