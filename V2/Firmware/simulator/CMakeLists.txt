cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(dive_watch_lvgl)
target_sources(app PRIVATE src/light_editor.c src/compass_ring.c src/appearance_checks.c src/battery_icon.c src/main.c src/scuba_model.c src/scuba_dashboard.c src/scuba_presentation.c src/scuba_ui.c src/scuba_checks.c src/compass_ui.c src/sim_dive.c src/watch_ui.c src/apnea_main.c src/apnea_stats.c src/apnea_settings.c src/apnea_checks.c src/apnea_settings_checks.c src/dive_layout.c src/sim_controls.c src/sim_checks.c src/v1_ui_layout.c src/selection.c
    src/assets/montserrat_60.c src/assets/montserrat_84.c src/assets/project_logo.c
    src/assets/icon_drop.c src/assets/icon_wave.c src/assets/icon_sun.c src/assets/turn_fish.c
    src/assets/icon_tank.c src/assets/icon_apnea.c src/assets/icon_scuba.c
    src/assets/icon_compass.c src/assets/icon_options.c src/assets/icon_watch.c)
target_sources(native_simulator INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src/sim_host.c)
# NCS 3.4.1 lists the GIF decoder but omits LVGL 9.5's moved GIF widget.
# Include that widget locally rather than modifying the installed SDK.
target_sources(app PRIVATE ${ZEPHYR_LVGL_MODULE_DIR}/src/widgets/gif/lv_gif.c)

# The calculated PC path runs in native_sim's host runner, using the same
# portable archive as the headless tests. No OSTC globals enter Zephyr/LVGL.
if(DIVE_BENCH)
    if(NOT EXISTS "${DIVE_OSTC4_LIBDIR}/libostc4_exposure.a")
        message(FATAL_ERROR "Build the OSTC4 exposure archive through launch.py --bench")
    endif()
    target_compile_definitions(app PRIVATE DIVE_PC_BENCH=1 OSTC4_TISSUE_CORRECTIONS=1)
    target_include_directories(app PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../ostc4_baseline/include")
    target_sources(app PRIVATE src/pc_bench_ui.c src/pc_bench_checks.c src/scuba_calculated.c)
    target_sources(native_simulator INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src/pc_bench.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/pc_bench_checkpoint_file.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/pc_bench_storage.c")
    target_compile_options(native_simulator INTERFACE -DDIVE_PC_BENCH=1 -DOSTC4_TISSUE_CORRECTIONS=1
        "-I${CMAKE_CURRENT_SOURCE_DIR}/../ostc4_baseline/include")
    set_property(TARGET native_simulator APPEND PROPERTY RUNNER_LINK_LIBRARIES
        "${DIVE_OSTC4_LIBDIR}/libostc4_exposure.a"
        "${DIVE_OSTC4_LIBDIR}/libostc4_upstream_contained.a")
    target_link_options(native_simulator INTERFACE -lm)
endif()
