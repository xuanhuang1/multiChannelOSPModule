## Copyright 2018-2020 Intel Corporation
## SPDX-License-Identifier: Apache-2.0

# build ospTutorial, for demonstration and API testing

# prefer libGL over libOpenGl for better compatibility with SWR
set(OpenGL_GL_PREFERENCE "LEGACY")
find_package(OpenGL 2 REQUIRED)
find_package(glfw3 REQUIRED)

# C++ version
add_executable(ospTutorial_mtvCpp
  ${OSPRAY_RESOURCE}
  multivariant.cpp
  ArcballCamera.cpp
  voxelGeneration.cpp
  imgui_impl_glfw_gl3.cpp
  TransferFunctionWidget.cpp
  Histogram.cpp
  clipping_plane.cpp
  )

target_link_libraries(ospTutorial_mtvCpp
  PRIVATE
  ospray_sdk
  ospray_imgui
  #glut
  glfw
  ${OPENGL_LIBRARIES}

  )
ospray_sign_target(ospTutorial_mtvCpp)

