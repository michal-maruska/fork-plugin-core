# So I don't use this:
# include(CTest)
# add_test(NAME test-circular COMMAND ExecutableToRun arg1 arg2 ...)

# this should be compatible with ctest(1).
include(GoogleTest)
include(../CodeCoverage.cmake)

include_directories(.)

# this was crucial to use GTest::gtest_main
find_package(GTest REQUIRED)
# find_package(GMock REQUIRED)
# include(GTest/GTestTargets)
append_coverage_compiler_flags()

add_executable(test-circular test-circular.cpp)
target_link_libraries(test-circular GTest::gtest_main)

gtest_discover_tests(test-circular)


add_executable(test-queue test-queue.cpp)
target_link_libraries(test-queue GTest::gtest_main)
target_include_directories(test-queue SYSTEM PUBLIC
  ${xorg-server_INCLUDE_DIRS}
  "../include/")

# here we also use fork_requests to get archive_events:
target_include_directories(test-queue SYSTEM PUBLIC ${xorg-server_INCLUDE_DIRS})

gtest_discover_tests(test-queue)


find_package(PkgConfig)
pkg_search_module(xorg-server REQUIRED xorg-server
  IMPORTED_TARGET GLOBAL)

add_executable(test-machine test-machine.cpp ../src/machine.cpp)
target_include_directories(test-machine SYSTEM PUBLIC
  ${xorg-server_INCLUDE_DIRS}
  "../include/")

#include_directories()
target_link_libraries(test-machine GTest::gtest_main)
target_link_libraries(test-machine GTest::gmock_main)

gtest_discover_tests(test-machine)


