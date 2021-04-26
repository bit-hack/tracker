include_directories(${CMAKE_CURRENT_SOURCE_DIR}/imgui)

add_library(imgui
  imgui/imgui.cpp
  imgui/imgui.h
  imgui/imconfig.h
  imgui/imgui_demo.cpp
  imgui/imgui_draw.cpp
  imgui/imgui_internal.h
  imgui/imgui_tables.cpp
  imgui/imgui_widgets.cpp
  imgui/imstb_rectpack.h
  imgui/imstb_textedit.h
  imgui/imstb_truetype.h

  imgui/backends/imgui_impl_opengl2.cpp
  imgui/backends/imgui_impl_opengl2.h

  imgui/backends/imgui_impl_sdl.cpp
  imgui/backends/imgui_impl_sdl.h
)

target_include_directories(
  imgui
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/imgui)
