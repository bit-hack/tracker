cmake_minimum_required(VERSION 3.2)
project(tracker)

find_package(OpenGL REQUIRED)

#find_package(SDL2 REQUIRED)
set(SDL2_INCLUDE_DIRS "INVALID" CACHE PATH "SDL2 Include Path")
set(SDL2_LIBRARIES "INVALID" CACHE FILEPATH "SDL2 Library File")
include_directories(${SDL2_INCLUDE_DIRS})

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/imgui)
add_subdirectory(external)

file(GLOB GAME_SRC "source/*.cpp" "source/*.h")
add_executable(tracker ${GAME_SRC})

target_link_libraries(
  tracker
  imgui ${SDL2_LIBRARIES} ${OPENGL_LIBRARIES})
