# Ajoute les drivers à la bibliothèque principale
target_sources(${COMPONENT_LIB} PRIVATE
    "oled_display.c"
    "battery.c"
   
    
    "pressure_sensor.c"
    "accelerometer.c"
)