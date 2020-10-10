project(shadertoy)

set(SOURCE_FILES
  shadertoy.cxx

  imgui.cpp
  imgui_draw.cpp
  imgui_widgets.cpp
  imgui_impl_glfw.cpp
  imgui_impl_opengl3.cpp  
)

set_source_files_properties(shadertoy.cxx PROPERTIES COMPILE_FLAGS -shader)

add_executable(shadertoy ${SOURCE_FILES})

target_link_libraries(shadertoy
  glfw
  gl3w
  GL
)