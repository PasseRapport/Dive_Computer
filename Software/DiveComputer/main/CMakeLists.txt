# On ne garde que la racine du code ici
set(SOURCES "main.c")

# Enregistrement avec les chemins d'inclusion pour que VS Code trouve les .h
idf_component_register(SRCS "${SOURCES}"
                       INCLUDE_DIRS "." "components" "ui"
                       REQUIRES esp_lcd lvgl driver esp_timer)

# On délègue le remplissage des sources aux sous-dossiers
add_subdirectory(components)
add_subdirectory(ui)
add_subdirectory(images)