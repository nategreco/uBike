# Universal Bike Controller
# Copyright (C) 2022-2023, Greco Engineering Solutions LLC
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

# Get the current working branch
execute_process(
    COMMAND git rev-parse --abbrev-ref HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_BRANCH
    OUTPUT_STRIP_TRAILING_WHITESPACE)

# Get the latest commit hash
execute_process(
    COMMAND git rev-parse HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE)

add_compile_definitions( GIT_COMMIT_HASH="${GIT_COMMIT_HASH}" 
    GIT_BRANCH="${GIT_BRANCH}" )

cmake_minimum_required(VERSION 3.20.0)
list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ubike)
target_include_directories(app PRIVATE include)

target_sources(app PRIVATE src/asciiModbus.c)
target_sources(app PRIVATE src/bikeControl.c)
target_sources(app PRIVATE src/cps.c)
target_sources(app PRIVATE src/cscs.c)
target_sources(app PRIVATE src/display.c)
# target_sources(app PRIVATE src/fec.c)
target_sources(app PRIVATE src/ftms.c)
target_sources(app PRIVATE src/main.c)