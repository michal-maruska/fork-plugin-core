* Build with cmake:


mkdir build
cd build
cmake -DCMAKE_COLOR_DIAGNOSTICS=1 -DCMAKE_BUILD_TYPE=Debug -G Ninja  ../  -DFORCE_COLORED_OUTPUT=1
--debug-trycompile

cmake -DCMAKE_BUILD_TYPE=Debug -G Ninja  ../  -DFORCE_COLORED_OUTPUT=1 -DCMAKE_CXX_COMPILER=clang++

cmake --build .
sudo cmake --install .
or:

ninja
sudo ninja  install

(cd test/; ctest )

** Get coverage report:

lcov -c -d . -o main_coverage.info
*** But I don't want to include c++/14/bits/ ....
lcov --exclude '**/boost/*/*.hpp'  --exclude '/usr/include/**' --exclude  '**/c++/14/**/*.h'  --exclude $(realpath ..)/test/\*   -c -d .   -o main_coverage.info

genhtml main_coverage.info --output-directory out

Overall coverage rate:
  lines......: 53.6% (1238 of 2310 lines)
  functions..: 42.4% (722 of 1703 functions)

# for some IDE:
ln -s build/compile_commands.json .

*** clang for the profiling libs
ai libclang-rt-dev


* debug:
script zapis  -c "ninja -v"


* Xorg headers files not clean to be used by C++ compiler

** min/max macros

/usr/include/xorg/misc.h
#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))
from...
/usr/include/xorg/misc.h
/usr/include/xorg/gc.h
/usr/include/xorg/dix.h
/usr/include/xorg/dixstruct.h
/usr/include/xorg/inputstr.h




** wrong files picked by:
#include <math.h>
#include <stdlib.h>

*** fix in:
/usr/include/xorg/os.h
/usr/include/xorg/misc.h

#define _GLIBCXX_INCLUDE_NEXT_C_HEADERS 1
#include
#undef _GLIBCXX_INCLUDE_NEXT_C_HEADERS


sudo chown michal  /usr/include/xorg/misc.h


* bugs:
error: 'KeyCode machineRec::forkActive [256]' is private within this context

error: redeclaration of 'Time machineRec::mCurrent_time'


* CMake Error: INSTALL(EXPORT) given unknown export "EXPORT_module"

* ctest:
ai libgtest-dev

g++ test-circular.cpp
g++ test-circular.cpp  -lgtest

cd tests; ctest

# now that I  moved enable_testing() to the root:
cmake --build . -t test


* invoke single target:
ninja src/CMakeFiles/fork.dir/history.cpp.o

ai libgmock-dev

https://www.reddit.com/r/cpp/comments/iwbdx7/is_ctest_worth_the_effort/


If you use gtest_discover_tests from the GoogleTest CMake module instead
of add_test then every unit test will be added as a separate CTest test
with its full name (MyTest.TestA). This makes it possible to execute
exactly one unit test by using ctest -R MyTest.TestA.



* idea for: C & C++
#ifdef __cplusplus
#define EXTERNC extern "C"
#else
#define EXTERNC
#endif

EXTERNC void mylibrary_mytype_doit(mylibrary_mytype_t self, int param);

#undef EXTERNC

* why extern?


* missing symbols:
- explicit template instantiation
- at runtime crash --- still missing symbols?

nm --undefined-only  src/libfork.so  |grep release_p

why does it simply exit?




/usr/lib/xorg/Xorg: symbol lookup error: /usr/lib/xorg/modules/fork.so: undefined symbol: detail_of

objdump -x  src/CMakeFiles/fork.dir/event_ops.cpp.o
so it's C++

00000000000000a5 g     F .text  0000000000000011 .hidden _Z9detail_ofPK14_InternalEvent

** objdump -t  src/CMakeFiles/fork.dir/event_ops.cpp.o


only _X_EXPORT makes visible!
all other .hidden
