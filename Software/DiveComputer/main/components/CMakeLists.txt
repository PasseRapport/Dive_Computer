# Ajoute les drivers à la bibliothèque principale
target_sources(${COMPONENT_LIB} PRIVATE
    "oled_display.c"
    "lps28dfw_reg.c"
    "sensor_manager.c"
)