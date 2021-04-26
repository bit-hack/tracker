#include <vector>

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_opengl.h>

#include "imgui.h"
#include "backends/imgui_impl_opengl2.h"
#include "backends/imgui_impl_sdl.h"


static int32_t _width = 640;
static int32_t _height = 480;

static SDL_Window *_window;
static SDL_GLContext _context;

static bool _active = true;

bool app_init() {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO) != 0) {
    return false;
  }

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

  _window = SDL_CreateWindow("tracker", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, _width, _height, SDL_WINDOW_OPENGL);
  if (!_window) {
    return false;
  }

  _context = SDL_GL_CreateContext(_window);
  if (!_context) {
    return false;
  }
  SDL_GL_MakeCurrent(_window, _context);

  // turn on vsync
  SDL_GL_SetSwapInterval(1);
  return true;
}

bool imgui_init() {
  ImGui::CreateContext();

  ImGui::StyleColorsDark();

  ImGui_ImplSDL2_InitForOpenGL(_window, _context);
  ImGui_ImplOpenGL2_Init();

  return true;
}

bool app_events() {

  SDL_Event event;
  while (SDL_PollEvent(&event)) {

    ImGui_ImplSDL2_ProcessEvent(&event);

    switch (event.type) {
    case SDL_QUIT:
      _active = false;
      break;
    }
  }

  return true;
}

int main() {

  SDL_SetMainReady();

  if (!app_init()) {
    return -1;
  }

  if (!imgui_init()) {
    return -1;
  }

  while (_active) {
    if (!app_events()) {
      break;
    }

    ImGui_ImplOpenGL2_NewFrame();
    ImGui_ImplSDL2_NewFrame(_window);
    ImGui::NewFrame();

    ImGui::Begin("Tests");

    ImGui::End();

    ImGui::Render();
    ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());

    SDL_GL_SwapWindow(_window);
  }

  return 0;
}
