cmake_minimum_required (VERSION 3.8)

project(test_tag)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -O3")
# add_compile_options(-Wall -Ofast -std=c++2a -march=skylake -flto -fno-exceptions -fno-rtti -fno-unwind-tables -fno-asynchronous-unwind-tables)


include_directories(../src)
aux_source_directory(../src DIR_LIB_SRCS)
add_executable(test test.cc ${DIR_LIB_SRCS})
target_link_libraries(test rt pthread)