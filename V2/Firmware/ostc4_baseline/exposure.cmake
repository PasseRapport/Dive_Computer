# SPDX-License-Identifier: GPL-3.0-or-later
# Shared PC/Zephyr production path. Pinned source and arithmetic stay identical.
set(ostc4_root "${CMAKE_CURRENT_LIST_DIR}")
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(corrected_decom "${CMAKE_CURRENT_BINARY_DIR}/corrected/decom.c")
add_custom_command(OUTPUT "${corrected_decom}"
    COMMAND ${Python3_EXECUTABLE} ${ostc4_root}/corrections/apply.py
        ${ostc4_root} "${corrected_decom}"
    DEPENDS ${ostc4_root}/corrections/apply.py
        ${ostc4_root}/corrections/tissue-corrections.json
        ${ostc4_root}/corrections/schreiner-original.txt
        ${ostc4_root}/corrections/schreiner-stable.c.txt
        ${ostc4_root}/upstream/Common/Src/decom.c
        ${ostc4_root}/upstream-manifest.json
    VERBATIM)
add_custom_target(ostc4_corrected_source DEPENDS "${corrected_decom}")
set(contained_planner "${CMAKE_CURRENT_BINARY_DIR}/corrected/buehlmann-contained.c")
add_custom_command(OUTPUT "${contained_planner}"
    COMMAND ${Python3_EXECUTABLE} ${ostc4_root}/corrections/contain_planner.py
        ${ostc4_root}/upstream/Discovery/Src/buehlmann.c "${contained_planner}"
    DEPENDS "${ostc4_root}/corrections/contain_planner.py" "${ostc4_root}/upstream/Discovery/Src/buehlmann.c" "${ostc4_root}/src/ostc4_scratch.h" "${ostc4_root}/upstream-manifest.json"
    VERBATIM)
set(grid_timed_planner "${CMAKE_CURRENT_BINARY_DIR}/corrected/buehlmann-grid-timed.c")
add_custom_command(OUTPUT "${grid_timed_planner}"
    COMMAND ${Python3_EXECUTABLE} ${ostc4_root}/corrections/correct_planner_timing.py
        "${contained_planner}" "${grid_timed_planner}"
    DEPENDS "${contained_planner}" "${ostc4_root}/corrections/correct_planner_timing.py"
        "${ostc4_root}/corrections/planner-timing-corrections.json"
    VERBATIM)
add_library(ostc4_upstream_contained STATIC "${corrected_decom}" "${grid_timed_planner}")
add_dependencies(ostc4_upstream_contained ostc4_corrected_source)
target_include_directories(ostc4_upstream_contained SYSTEM PUBLIC "${ostc4_root}/compat" "${ostc4_root}/upstream/Common/Inc" "${ostc4_root}/upstream/Discovery/Inc")
target_include_directories(ostc4_upstream_contained PRIVATE "${ostc4_root}/src")
target_compile_options(ostc4_upstream_contained PRIVATE -O2 -fno-fast-math -ffp-contract=off -ffunction-sections -fdata-sections)
add_library(ostc4_exposure STATIC "${ostc4_root}/src/ostc4_baseline.c")
target_compile_definitions(ostc4_exposure PUBLIC OSTC4_TISSUE_CORRECTIONS=1 PRIVATE OSTC4_CONTINUOUS_EXPOSURE=1)
target_include_directories(ostc4_exposure PUBLIC "${ostc4_root}/include")
target_link_libraries(ostc4_exposure PRIVATE ostc4_upstream_contained m)
target_link_options(ostc4_exposure INTERFACE -Wl,--gc-sections)
target_compile_options(ostc4_exposure PRIVATE -O2 -Wall -Wextra -Wpedantic -Werror -fno-fast-math -ffp-contract=off)

if(TARGET zephyr_interface)
    target_link_libraries(ostc4_upstream_contained PRIVATE zephyr_interface)
    target_link_libraries(ostc4_exposure PRIVATE zephyr_interface)
endif()
