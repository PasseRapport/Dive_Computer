cmake_minimum_required(VERSION 3.20)
project(pc_bench_checks C)
enable_testing()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_subdirectory(../../../ostc4_baseline ostc4)
add_executable(test_pc_bench test_pc_bench.c ../../src/pc_bench.c ../../src/pc_bench_storage.c ../../src/scuba_calculated.c ../../src/scuba_model.c)
target_include_directories(test_pc_bench PRIVATE ../../src)
target_link_libraries(test_pc_bench PRIVATE ostc4_exposure m)
target_compile_options(test_pc_bench PRIVATE -Wall -Wextra -Wpedantic -Werror -fno-fast-math -ffp-contract=off)
add_test(NAME pc_bench_controls_time_graph COMMAND test_pc_bench)

add_executable(test_calculated_watch test_calculated_watch.c ../../src/scuba_calculated.c ../../src/scuba_model.c)
target_include_directories(test_calculated_watch PRIVATE ../../src)
target_link_libraries(test_calculated_watch PRIVATE ostc4_exposure m)
target_compile_options(test_calculated_watch PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME calculated_watch_mapping_session_alerts COMMAND test_calculated_watch)

add_executable(test_restart_bench test_restart_bench.c ../../src/pc_bench.c ../../src/pc_bench_storage.c ../../src/scuba_calculated.c ../../src/scuba_model.c)
target_include_directories(test_restart_bench PRIVATE ../../src)
target_link_libraries(test_restart_bench PRIVATE ostc4_exposure m)
target_compile_options(test_restart_bench PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME restart_watch_diagnostics COMMAND test_restart_bench)

add_executable(test_gf_pair test_gf_pair.c ../../src/pc_bench.c ../../src/pc_bench_storage.c)
target_include_directories(test_gf_pair PRIVATE ../../src)
target_link_libraries(test_gf_pair PRIVATE ostc4_exposure m)
target_compile_options(test_gf_pair PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME configurable_gf_public_behavior COMMAND test_gf_pair)

add_executable(gf_bench_replay gf_bench_replay.c ../../src/pc_bench.c ../../src/pc_bench_storage.c)
target_include_directories(gf_bench_replay PRIVATE ../../src)
target_link_libraries(gf_bench_replay PRIVATE ostc4_exposure m)
target_compile_options(gf_bench_replay PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME configurable_gf_independent_acceptance COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/compare_gf_reference.py $<TARGET_FILE:gf_bench_replay>)

add_executable(test_checkpoint_file test_checkpoint_file.c ../../src/pc_bench.c ../../src/pc_bench_checkpoint_file.c ../../src/pc_bench_storage.c)
target_include_directories(test_checkpoint_file PRIVATE ../../src)
target_link_libraries(test_checkpoint_file PRIVATE ostc4_exposure m)
target_compile_options(test_checkpoint_file PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME disk_checkpoint_save COMMAND test_checkpoint_file save ${CMAKE_CURRENT_BINARY_DIR}/locked.checkpoint)
set_tests_properties(disk_checkpoint_save PROPERTIES FIXTURES_SETUP locked_checkpoint)
add_test(NAME disk_checkpoint_fresh_process COMMAND test_checkpoint_file load ${CMAKE_CURRENT_BINARY_DIR}/locked.checkpoint)
set_tests_properties(disk_checkpoint_fresh_process PROPERTIES FIXTURES_REQUIRED locked_checkpoint)
add_test(NAME disk_checkpoint_complete_later_exposure COMMAND test_checkpoint_file continued ${CMAKE_CURRENT_BINARY_DIR}/locked.checkpoint)
set_tests_properties(disk_checkpoint_complete_later_exposure PROPERTIES FIXTURES_REQUIRED locked_checkpoint)
add_test(NAME disk_checkpoint_requires_configuration_evidence COMMAND test_checkpoint_file configuration-unknown ${CMAKE_CURRENT_BINARY_DIR}/locked.checkpoint)
set_tests_properties(disk_checkpoint_requires_configuration_evidence PROPERTIES FIXTURES_REQUIRED locked_checkpoint)
add_test(NAME disk_checkpoint_process_continuity_and_faults COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/check_checkpoint_disk.py $<TARGET_FILE:test_checkpoint_file>
    ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/legacy-fixed30-70-v1.bin)

add_executable(test_storage_bench test_storage_bench.c ../../src/pc_bench.c ../../src/pc_bench_storage.c)
target_include_directories(test_storage_bench PRIVATE ../../src)
target_link_libraries(test_storage_bench PRIVATE ostc4_exposure m)
target_compile_options(test_storage_bench PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME durable_preferences_logs_separate_processes COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/check_storage_processes.py $<TARGET_FILE:test_storage_bench>)

add_test(NAME independent_exact_stop_grid_travel COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/test_nominal_stop_travel.py $<TARGET_FILE:gf_bench_replay>)
