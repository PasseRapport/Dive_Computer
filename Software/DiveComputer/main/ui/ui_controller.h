#ifndef UI_CONTROLLER_H
#define UI_CONTROLLER_H

#include <stdint.h>

// Lance tous les timers système de l'UI (ex: Run Time)
void init_ui_controllers(void);

// Appelé quand l'utilisateur change la luminosité via le menu
void handle_brightness_change(uint8_t new_level);

#endif // UI_CONTROLLER_H