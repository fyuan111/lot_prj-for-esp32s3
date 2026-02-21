cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
    "src/HW"
    "src/APP"
    "src/SYSTEM")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(lot_prj)
