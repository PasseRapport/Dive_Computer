#include "water_settings.h"
static bool salt_water=true, scuba_salt=true, scuba_active, apnea_active;
bool water_settings_salt(void) { return scuba_active?scuba_salt:salt_water; }
bool water_settings_preferred_salt(void) { return salt_water; }
void water_settings_recover_scuba(bool salt,bool active) { scuba_salt=salt;scuba_active=active; }
bool water_settings_locked(void) { return scuba_active || apnea_active; }
int water_settings_select(bool salt)
{
    if(water_settings_locked())return -1;
    salt_water=salt;return 0;
}
void water_settings_restore(bool salt) { salt_water=salt; }
void water_settings_lock_scuba(bool active)
{ if(active && !scuba_active)scuba_salt=salt_water;scuba_active=active; }
void water_settings_lock_apnea(bool active) { apnea_active=active; }
