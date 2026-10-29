#ifndef WATER_SETTINGS_H
#define WATER_SETTINGS_H
#include <stdbool.h>
bool water_settings_salt(void);
bool water_settings_preferred_salt(void);
/* Restore an active recording without replacing the confirmed next choice. */
void water_settings_recover_scuba(bool salt,bool active);
bool water_settings_locked(void);
int water_settings_select(bool salt);
/* Startup/explicit fresh fixture restoration; does not change lock state. */
void water_settings_restore(bool salt);
void water_settings_lock_scuba(bool active);
void water_settings_lock_apnea(bool active);
#endif
