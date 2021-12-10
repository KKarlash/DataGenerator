cmake_minimum_required(VERSION 3.10)

# set the project name and version
project(DataGenerator VERSION 1.0)

find_package(PkgConfig REQUIRED)
pkg_check_modules(Mosquitto REQUIRED IMPORTED_TARGET libmosquitto)


set(HEADER_FILES mosquitto.h)

# add the executable
add_executable(main main.cpp)

