cmake_minimum_required(VERSION 3.16)
project(ostc4_baseline VERSION 0.1 LANGUAGES C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang" OR NOT UNIX OR APPLE)
    message(FATAL_ERROR "This unchanged baseline requires a GNU-compatible ELF toolchain and libm")
endif()
add_library(ostc4_upstream STATIC upstream/Common/Src/decom.c upstream/Discovery/Src/buehlmann.c)
target_include_directories(ostc4_upstream SYSTEM PUBLIC compat upstream/Common/Inc upstream/Discovery/Inc)
target_compile_options(ostc4_upstream PRIVATE -O2 -fno-fast-math -ffp-contract=off -ffunction-sections -fdata-sections)
add_library(ostc4_baseline STATIC src/ostc4_baseline.c)
target_include_directories(ostc4_baseline PUBLIC include)
target_link_libraries(ostc4_baseline PRIVATE ostc4_upstream m)
target_link_options(ostc4_baseline INTERFACE -Wl,--gc-sections)
target_compile_options(ostc4_baseline PRIVATE -O2 -Wall -Wextra -Wpedantic -Werror -fno-fast-math -ffp-contract=off)
add_executable(ostc4_replay src/replay.c)
target_link_libraries(ostc4_replay PRIVATE ostc4_baseline m)
target_compile_options(ostc4_replay PRIVATE -O2 -Wall -Wextra -Wpedantic -Werror -fno-fast-math -ffp-contract=off)
include(CTest)
if(BUILD_TESTING)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(NAME upstream_source_identity COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_source_identity.py ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME synthetic_replay_contracts COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_replay.py $<TARGET_FILE:ostc4_replay> ${CMAKE_CURRENT_SOURCE_DIR})
    add_executable(test_ostc4_contract tests/test_contract.c)
    target_link_libraries(test_ostc4_contract PRIVATE ostc4_baseline)
    target_compile_options(test_ostc4_contract PRIVATE -Wall -Wextra -Wpedantic -Werror)
    add_test(NAME public_pressure_time_contract COMMAND test_ostc4_contract)
endif()

# Build-time comparison variant; the pinned upstream tree remains byte-identical.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(corrected_decom "${CMAKE_CURRENT_BINARY_DIR}/corrected/decom.c")
add_custom_command(OUTPUT "${corrected_decom}"
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/corrections/apply.py
        ${CMAKE_CURRENT_SOURCE_DIR} "${corrected_decom}"
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/corrections/apply.py
        ${CMAKE_CURRENT_SOURCE_DIR}/corrections/tissue-corrections.json
        ${CMAKE_CURRENT_SOURCE_DIR}/corrections/schreiner-original.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/corrections/schreiner-stable.c.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/upstream/Common/Src/decom.c
        ${CMAKE_CURRENT_SOURCE_DIR}/upstream-manifest.json
    VERBATIM)
add_library(ostc4_upstream_corrected STATIC "${corrected_decom}" upstream/Discovery/Src/buehlmann.c)
target_include_directories(ostc4_upstream_corrected SYSTEM PUBLIC compat upstream/Common/Inc upstream/Discovery/Inc)
target_compile_options(ostc4_upstream_corrected PRIVATE -O2 -fno-fast-math -ffp-contract=off -ffunction-sections -fdata-sections)
add_library(ostc4_corrected STATIC src/ostc4_baseline.c)
target_compile_definitions(ostc4_corrected PUBLIC OSTC4_TISSUE_CORRECTIONS=1)
target_include_directories(ostc4_corrected PUBLIC include)
target_link_libraries(ostc4_corrected PRIVATE ostc4_upstream_corrected m)
target_link_options(ostc4_corrected INTERFACE -Wl,--gc-sections)
target_compile_options(ostc4_corrected PRIVATE -O2 -Wall -Wextra -Wpedantic -Werror -fno-fast-math -ffp-contract=off)
add_executable(ostc4_replay_corrected src/replay.c)
target_link_libraries(ostc4_replay_corrected PRIVATE ostc4_corrected m)
target_compile_options(ostc4_replay_corrected PRIVATE -O2 -Wall -Wextra -Wpedantic -Werror -fno-fast-math -ffp-contract=off)
if(BUILD_TESTING)
    add_executable(test_ostc4_corrected_contract tests/test_contract.c)
    target_link_libraries(test_ostc4_corrected_contract PRIVATE ostc4_corrected)
    target_compile_options(test_ostc4_corrected_contract PRIVATE -Wall -Wextra -Wpedantic -Werror)
    add_test(NAME corrected_public_pressure_time_contract COMMAND test_ostc4_corrected_contract)
    add_test(NAME corrected_synthetic_replay_contracts COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_replay.py $<TARGET_FILE:ostc4_replay_corrected> ${CMAKE_CURRENT_SOURCE_DIR})
endif()

if(BUILD_TESTING)
    add_test(NAME independently_verified_tissues COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/test_tissue_verification.py
        $<TARGET_FILE:ostc4_replay> $<TARGET_FILE:ostc4_replay_corrected>
        ${CMAKE_CURRENT_SOURCE_DIR}/upstream/Common/Src/decom.c "${corrected_decom}")
endif()
