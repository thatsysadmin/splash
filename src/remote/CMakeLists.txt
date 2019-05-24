#
# Copyright (C) 2018 Emmanuel Durand
#
# This file is part of Splash.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# Splash is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Splash.  If not, see <http://www.gnu.org/licenses/>.
#

project(
    splash-remote
)

if (NOT "${CMAKE_SYSTEM_NAME}" MATCHES "Emscripten")
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/webasm)
    execute_process(
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../external/emsdk/emscripten
        COMMAND sh -c "ls | sort | head -n 1 | tr -d '\n'" OUTPUT_VARIABLE LATEST_EMSDK
    )
    execute_process(
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/webasm
        COMMAND ${CMAKE_COMMAND} -DCMAKE_TOOLCHAIN_FILE=${CMAKE_CURRENT_SOURCE_DIR}/../../external/emsdk/emscripten/${LATEST_EMSDK}/cmake/Modules/Platform/Emscripten.cmake -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -G "Unix Makefiles" ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_custom_target(remote-webasm
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/webasm
        COMMAND ${CMAKE_MAKE_PROGRAM} -j$(nproc)
    )

    return()
endif()

cmake_minimum_required(VERSION 3.5)

set(CMAKE_EXECUTABLE_SUFFIX ".html")
set(CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s USE_GLFW=3 -s USE_WEBGL2=1 -s ALLOW_MEMORY_GROWTH=1 -s ASSERTIONS=1")

include_directories(../)
include_directories(../../external/imgui)

add_executable(remote-webasm remote.cpp)

target_sources(remote-webasm PRIVATE
    ../core/tree/tree_branch.cpp
    ../core/tree/tree_leaf.cpp
    ../core/tree/tree_root.cpp
    ../network/socket_client.cpp
    ../network/socket_message.cpp
    ../../external/imgui/imgui_demo.cpp
    ../../external/imgui/imgui_draw.cpp
    ../../external/imgui/imgui_widgets.cpp
    ../../external/imgui/imgui.cpp
)

target_link_libraries(remote-webasm glfw)
target_link_libraries(remote-webasm GL)
