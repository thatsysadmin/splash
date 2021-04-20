/*
 * Copyright (C) 2014 Emmanuel Durand
 *
 * This file is part of Splash.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Splash is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Splash.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * @constants.h
 * Splash-wide constants, and useful defines
 */

#ifndef SPLASH_CONSTANTS_H
#define SPLASH_CONSTANTS_H

#include "./config.h"

#define SPLASH
#define SPLASH_GL_DEBUG true

#define SPLASH_ALL_PEERS "__ALL__"
#define SPLASH_DEFAULTS_FILE_ENV "SPLASH_DEFAULTS"

#define SPLASH_FILE_CONFIGURATION "splashConfiguration"
#define SPLASH_FILE_PROJECT "splashProject"

#define SPLASH_CAMERA_LINK "__camera_link"

#define SPLASH_GRAPH_TYPE_CAMERA "camera"
#define SPLASH_GRAPH_TYPE_BLENDER "blender"
#define SPLASH_GRAPH_TYPE_COLORCALIBRATOR "colorCalibrator"
#define SPLASH_GRAPH_TYPE_DRAGNDROP "dragndrop"
#define SPLASH_GRAPH_TYPE_FILTER "filter"
#define SPLASH_GRAPH_TYPE_FILTER_BL "filter_black_level"
#define SPLASH_GRAPH_TYPE_FILTER_C "filter_custom"
#define SPLASH_GRAPH_TYPE_FILTER_CC "filter_color_curves"
#define SPLASH_GRAPH_TYPE_GEOMETRICCALIBRATOR "geometricCalibrator"
#define SPLASH_GRAPH_TYPE_GEOMETRY "geometry"
#define SPLASH_GRAPH_TYPE_GUI "gui"
#define SPLASH_GRAPH_TYPE_IMAGE "image"
#define SPLASH_GRAPH_TYPE_IMAGE_FFMPEG "image_ffmpeg"
#define SPLASH_GRAPH_TYPE_IMAGE_GPHOTO "image_gphoto"
#define SPLASH_GRAPH_TYPE_IMAGE_LIST "image_list"
#define SPLASH_GRAPH_TYPE_IMAGE_OPENCV "image_opencv"
#define SPLASH_GRAPH_TYPE_IMAGE_SEQUENCE "image_sequence"
#define SPLASH_GRAPH_TYPE_IMAGE_SHMDATA "image_shmdata"
#define SPLASH_GRAPH_TYPE_IMAGE_V4L2 "image_v4l2"
#define SPLASH_GRAPH_TYPE_JOYSTICK "joystick"
#define SPLASH_GRAPH_TYPE_KEYBOARD "keyboard"
#define SPLASH_GRAPH_TYPE_MESH "mesh"
#define SPLASH_GRAPH_TYPE_MESH_BEZIERPATCH "mesh_bezierPatch"
#define SPLASH_GRAPH_TYPE_MESH_SHMDATA "mesh_shmdata"
#define SPLASH_GRAPH_TYPE_MOUSE "mouse"
#define SPLASH_GRAPH_TYPE_OBJECT "object"
#define SPLASH_GRAPH_TYPE_PYTHON "python"
#define SPLASH_GRAPH_TYPE_QUEUE "queue"
#define SPLASH_GRAPH_TYPE_SHADER "shader"
#define SPLASH_GRAPH_TYPE_SINK "sink"
#define SPLASH_GRAPH_TYPE_SINK_SHMDATA "sink_shmdata"
#define SPLASH_GRAPH_TYPE_SINK_SHMDATAENCODED "sink_shmdata_encoded"
#define SPLASH_GRAPH_TYPE_TEXCOORDGENERATOR "texCoordGenerator"
#define SPLASH_GRAPH_TYPE_TEXTURE "texture"
#define SPLASH_GRAPH_TYPE_TEXTUREIMAGE "texture_image"
#define SPLASH_GRAPH_TYPE_USERINPUT "userinput"
#define SPLASH_GRAPH_TYPE_VIRTUALPROBE "virtual_probe"
#define SPLASH_GRAPH_TYPE_WARP "warp"
#define SPLASH_GRAPH_TYPE_WINDOW "window"

#define GL_VENDOR_NVIDIA "NVIDIA Corporation"
#define GL_VENDOR_AMD "X.Org"
#define GL_VENDOR_INTEL "Intel"

#define GL_TIMING_PREFIX "__gl_timing_"
#define GL_TIMING_TIME_PER_FRAME "time_per_frame"
#define GL_TIMING_TEXTURES_UPLOAD "texture_upload"
#define GL_TIMING_RENDERING "rendering"
#define GL_TIMING_SWAP "swap"

#include <execinfo.h>
#include <iostream>

// clang-format off
#include "./glad/glad.h"
#include <GLFW/glfw3.h>
// clang-format on

#define PRINT_FUNCTION_LINE std::cout << "------> " << __PRETTY_FUNCTION__ << "::" << __LINE__ << std::endl;

#define PRINT_CALL_STACK                                                                                                                                                           \
    {                                                                                                                                                                              \
        int j, nptrs;                                                                                                                                                              \
        int size = 100;                                                                                                                                                            \
        void* buffers[size];                                                                                                                                                       \
        char** strings;                                                                                                                                                            \
                                                                                                                                                                                   \
        nptrs = backtrace(buffers, size);                                                                                                                                          \
        strings = backtrace_symbols(buffers, nptrs);                                                                                                                               \
        for (j = 0; j < nptrs; ++j)                                                                                                                                                \
            std::cout << strings[j] << std::endl;                                                                                                                                  \
                                                                                                                                                                                   \
        free(strings);                                                                                                                                                             \
    }

#endif // SPLASH_CONSTANTS_H
