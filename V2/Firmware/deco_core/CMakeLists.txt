cmake_minimum_required(VERSION 3.16)
project(deco_core VERSION 1.0 LANGUAGES C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(deco_tissues STATIC src/deco_tissues.c src/deco_ceiling.c)
set_target_properties(deco_tissues PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(deco_tissues PUBLIC include)
target_link_libraries(deco_tissues PUBLIC m)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(deco_tissues PRIVATE -Wall -Wextra -Wpedantic -Werror
        -fno-fast-math -ffp-contract=off)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(test_deco_ceiling tests/test_deco_ceiling.c)
    target_link_libraries(test_deco_ceiling PRIVATE deco_tissues)
    add_executable(ceiling_cli tests/ceiling_cli.c)
    target_link_libraries(ceiling_cli PRIVATE deco_tissues)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        foreach(target test_deco_ceiling ceiling_cli)
            target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic
                -Werror -fno-fast-math -ffp-contract=off)
        endforeach()
    endif()
    add_test(NAME ceiling_contracts COMMAND test_deco_ceiling)
    add_test(NAME ceiling_cli_contracts
        COMMAND ${CMAKE_COMMAND} -DCLI=$<TARGET_FILE:ceiling_cli>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_ceiling_cli.cmake)
    add_executable(test_deco_tissues tests/test_deco_tissues.c)
    target_link_libraries(test_deco_tissues PRIVATE deco_tissues)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(test_deco_tissues PRIVATE -Wall -Wextra -Wpedantic
            -Werror -fno-fast-math -ffp-contract=off)
    endif()
    add_test(NAME tissues_analytical_transactional COMMAND test_deco_tissues)
    add_executable(tissue_segments_cli tests/tissue_segments_cli.c)
    target_link_libraries(tissue_segments_cli PRIVATE deco_tissues)
    add_test(NAME tissues_cli_duration_parsing
        COMMAND ${CMAKE_COMMAND} -DCLI=$<TARGET_FILE:tissue_segments_cli>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cli_duration.cmake)
    if(UNIX AND NOT APPLE AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        add_executable(test_deco_numerical_failure tests/test_deco_numerical_failure.c)
        target_link_libraries(test_deco_numerical_failure PRIVATE deco_tissues)
        target_link_options(test_deco_numerical_failure PRIVATE
            -Wl,--wrap=exp -Wl,--wrap=expm1)
        add_test(NAME tissues_atomic_numerical_failure COMMAND test_deco_numerical_failure)
    endif()
endif()
