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

if(false)
add_executable(test-queue test-queue.cpp)
target_link_libraries(test-queue GTest::gtest_main)
gtest_discover_tests(test-queue)
endif()

add_executable(test-triqueue test-triqueue.cpp)
target_link_libraries(test-triqueue GTest::gtest_main)
gtest_discover_tests(test-triqueue)



add_executable(test-machine test-machine.cpp)

target_link_libraries(test-machine PRIVATE forking_machine::forking_machine
    GTest::gtest_main
    GTest::gmock_main
)
# target_include_directories(test-machine SYSTEM PUBLIC "../include/")

option(ENABLE_ASAN "Build with AddressSanitizer" OFF)
option(ENABLE_TSAN "Build with ThreadSanitizer" OFF)

if(ENABLE_ASAN)
  target_compile_options(test-machine PRIVATE -fsanitize=address -fno-omit-frame-pointer -g)
  target_link_options(test-machine PRIVATE -fsanitize=address)
endif()

if(ENABLE_TSAN)
  target_compile_options(test-machine PRIVATE -fsanitize=thread -fno-omit-frame-pointer -g)
  target_link_options(test-machine PRIVATE -fsanitize=thread)
endif()

#include_directories()

gtest_discover_tests(test-machine)



