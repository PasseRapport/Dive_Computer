#ifndef UI_MODEL_H
#define UI_MODEL_H

#include <stdint.h>
#include <stdbool.h>

// --- Structure des données du système ---
// Ces variables seront lues par tes capteurs I2C et envoyées à l'écran
typedef struct {
    // --- Capteurs physiques ---
    float current_depth;
    float max_depth;
    float session_max_depth;
    float temperature;

    // --- Variables de Plongée (En immersion) ---
    bool is_diving;           // true si on est sous l'eau
    int64_t dive_start_us;    // Timestamp du début de la plongée
    uint32_t current_dive_s;  // Durée de l'apnée actuelle en secondes

    // --- Variables de Surface (Récupération et Stats) ---
    uint32_t last_dive_time_s;   // Durée de la dernière immersion
    uint32_t max_dive_time_s;    // Temps record de la session (pour l'écran Stats)
    uint32_t recovery_time_s;    // Chrono de surface
    bool has_dived_once;         // Pour savoir s'il faut afficher le chrono
    int64_t surface_start_us;    // Timestamp de sortie d'eau

    // --- Paramètres Généraux ---
    uint32_t total_dives;    // Nombre de plongées de la session
    bool is_salt_water;      // true = Mer, false = Eau Douce
    uint8_t brightness_lvl;  // Niveau actuel (0 à 5)
    uint8_t battery_percentage; // % de batterie (0-100)
} dive_state_t;

// Variable globale accessible partout
extern dive_state_t system_state;

#endif // UI_MODEL_H