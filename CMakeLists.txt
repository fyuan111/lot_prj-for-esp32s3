cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
    "src/HW"
    "src/APP"
    "src/SYSTEM")

# Project-wide forced include for launch marker macro.
# This avoids writing #include "launch.h" in every module file.
include_directories("${CMAKE_SOURCE_DIR}/src/SYSTEM/launch")
add_compile_options("$<$<COMPILE_LANGUAGE:C>:-include>"
                    "$<$<COMPILE_LANGUAGE:C>:launch.h>")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(lot_prj)
