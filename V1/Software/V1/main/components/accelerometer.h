#ifndef ACCELEROMETER_H
#define ACCELEROMETER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/**
 * @brief Initialise le bus I2C (si nécessaire), configure la broche d'interruption INT1,
 * charge le programme FSM dans la mémoire de l'accéléromètre LIS2DUX12,
 * et lance la tâche de détection en arrière-plan.
 */
void init_accelerometer(void);
esp_err_t get_acceleration(int16_t *x, int16_t *y, int16_t *z);

#endif // ACCELEROMETER_H