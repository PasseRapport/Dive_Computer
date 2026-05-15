#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <stdint.h>

// Initialise le bus I2C et configure le capteur LPS28DFW
void init_dive_sensor(void);
void calibrate_surface_pressure(void);
// Tâche FreeRTOS qui va lire le capteur en boucle
void dive_sensor_task(void *pvParameters);

#endif // SENSOR_MANAGER_H