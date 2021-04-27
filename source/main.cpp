#include <vector>
#include <memory>

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_opengl.h>

#include "imgui.h"
#include "backends/imgui_impl_opengl2.h"
#include "backends/imgui_impl_sdl.h"

#include "tracker.h"


static int32_t _width = 512;
static int32_t _height = 512;

static SDL_Window *_window;
static SDL_GLContext _context;

static bool _active = true;

static std::unique_ptr<Tracker::song_t> _song;
static std::unique_ptr<Tracker::player_t> _player;

static int _gui_pattern = 0;
static int _gui_instrument = 0;


void audio_callback(void *user, uint8_t *data, int size) {
  memset(data, 0, size);

  if (!_player) {
    return;
  }

  std::array<int16_t, 1024> temp;

  // number of samples we need total
  uint32_t samples = size / (sizeof(int16_t) * 2);
  // output stream
  int16_t *out = (int16_t *)data;

#if 0
  static float f = 0.f;
  for (int i = 0; i < samples; ++i) {
    int16_t s = int16_t(sinf(f) * 0x1fff);
    out[0] = s;
    out[1] = s;
    out += 2;
    f += ((2.f * M_PI) * 440.f) / 44100.f;
  }
#endif

#if 1
  // while there are samples to render
  while (samples) {
    // number of samples we can do in one sitting
    const uint32_t todo = std::min<uint32_t>(samples, uint32_t(temp.size()));
    // render from the player
    temp.fill(0);
    _player->render(temp.data(), todo);
    // render to mono for the output stream
    for (uint32_t i = 0; i < todo; ++i) {
      out[0] = temp[i];
      out[1] = temp[i];
      out += 2;
    }
    samples -= todo;
  }
#endif
}

bool audio_init() {

  SDL_AudioSpec desired, spec;
  memset(&spec, 0, sizeof(spec));
  memset(&desired, 0, sizeof(desired));

  desired.size = sizeof(desired);
  desired.channels = 2;
  desired.freq = 44100;
  desired.samples = 1024*4;  // ~100ms
  desired.format = AUDIO_S16SYS;
  desired.callback = audio_callback;
  desired.userdata = nullptr;

  if (SDL_OpenAudio(&desired, nullptr) < 0) {
    return false;
  }

  SDL_PauseAudio(SDL_FALSE);
  return true;
}

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

void visit_instrument() {
  if (!_song) {
    return;
  }
  auto &ins = _song->instruments[_gui_instrument];
  ImGui::Begin("Instrument");
  if (ImGui::Button("Generate")) {
    auto &s = ins.sample;
    s.size = 11050 * 4;
    s.data.reset(new int16_t[s.size]);
    s.sample_rate = 22050;
    float x = 0.f;
    float step = 2.f * float(M_PI) / (s.sample_rate / 440);
    for (uint32_t i = 0; i < s.size; ++i) {
      s.data[i] = int16_t(sinf(x) * 0x1fff);
      x += step;
    }
    ins.sample_start = 0;
    ins.sample_end = s.size;
  }
  {
    int ss = ins.sample_start;
    ImGui::SliderInt("Sample Start", &ss, 0, ins.sample.size-1);
    ins.sample_start = ss;
  }
  {
    int se = ins.sample_end;
    ImGui::SliderInt("Sample End", &se, 0, ins.sample.size-1);
    ins.sample_end = se;
  }
  {
    int root = ins.root;
    ImGui::SliderInt("Root", &root, 1, 127);
    ins.root = root;
  }
  {
    ImGui::SliderFloat("Fine", &ins.fine, -1.f, 1.f);
  }
  {
    ImGui::Text("Sample Rate %d", int(ins.sample.sample_rate));
  }
  ImGui::End();
}

void visit_pattern() {
  if (!_song) {
    return;
  }
  auto &pat = _song->patterns[_gui_pattern];
  ImGui::Begin("Pattern");


  ImGui::End();
}

void visit_song() {
  if (!_song) {
    return;
  }
  ImGui::Begin("Song");
  // do BPM stuff
  {
    int bpm = _song->bpm;
    ImGui::SliderInt("BPM", &bpm, 40, 180);
    _song->bpm = bpm;
  }
  {
    ImGui::SliderInt("Pattern", &_gui_pattern, 0, Tracker::MAX_PATTERNS-1);
  }
  {
    ImGui::SliderInt("Instrument", &_gui_instrument, 0, Tracker::MAX_INSTUMENTS-1);
  }
  ImGui::End();
}

void visit_player() {
  if (!_player) {
    return;
  }
  ImGui::Begin("Player");
  if (ImGui::Button("Play")) {
    _player->play();
  }
  if (ImGui::Button("Stop")) {
    _player->stop();
  }
  ImGui::End();
}

void tick() {
  visit_song();
  visit_player();
  visit_instrument();
  visit_pattern();
}

int main() {
  SDL_SetMainReady();
  if (!app_init()) {
    return -1;
  }
  if (!imgui_init()) {
    return -1;
  }

  _song.reset(new Tracker::song_t);
  {
    auto &pat = _song->patterns[0];
    pat.note_insert(Tracker::note_t{ 0,  69 + 12, 0 });
    pat.note_insert(Tracker::note_t{ 8,  69,      0 });
    pat.note_insert(Tracker::note_t{ 12, 69,      0 });
    pat.note_insert(Tracker::note_t{ 4,  69,      0 });
  }
  _player.reset(new Tracker::player_t(*_song.get(), 44100));

  audio_init();

  while (_active) {
    if (!app_events()) {
      break;
    }

    glClearColor(.2f, .4f, .6f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL2_NewFrame();
    ImGui_ImplSDL2_NewFrame(_window);
    ImGui::NewFrame();
    tick();
    ImGui::Render();
    ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(_window);
    SDL_Delay(1);
  }
  return 0;
}
