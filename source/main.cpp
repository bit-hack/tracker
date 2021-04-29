#include <vector>
#include <memory>
#include <map>
#include <string>

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_opengl.h>

#include "imgui.h"
#include "backends/imgui_impl_opengl2.h"
#include "backends/imgui_impl_sdl.h"

#include "tracker.h"
#include "libwav.h"


static int32_t _width = 1024;
static int32_t _height = 768;

static SDL_Window *_window;
static SDL_GLContext _context;

static bool _active = true;

static std::unique_ptr<Tracker::song_t> _song;
static std::unique_ptr<Tracker::player_t> _player;

static int _gui_pattern = 0;
static int _gui_instrument = 0;

static std::map<std::string, wave_t> _samples;


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

void load_samples() {
  _samples.clear();
  WIN32_FIND_DATAA find;
  HANDLE handle = FindFirstFileA("./samples/*.wav", &find);
  if (handle == INVALID_HANDLE_VALUE) {
    return;
  }
  do {
    std::string name = std::string("./samples/") + find.cFileName;
    wave_t wave;
    if (wave.load(name.c_str())) {
      _samples[name] = std::move(wave);
    }
  } while (FindNextFileA(handle, &find));
}

void visit_samples() {
  ImGui::Begin("Samples");
  ImGui::BeginChild("SamplesScrollBox");
  for (const auto &s : _samples) {
    if (!ImGui::Selectable(s.first.c_str())) {
      continue;
    }
    const auto &sample = s.second;
    {
      uint32_t sample_size = sample.num_frames();

      std::lock_guard<std::mutex> guard{ _player->mutex() };
      auto &ins = _song->instruments[_gui_instrument];
      ins.sample.data.reset(new int16_t[sample_size]);
      ins.sample_start = 0;
      ins.sample_end = sample_size;
      ins.sample.size = sample_size;
      ins.sample.sample_rate = sample.sample_rate();
      // copy over the sample
      for (int i = 0; i < sample_size; ++i) {
        ins.sample.data[i] = sample.get_sample(i, 0);
      }
    }
  }
  ImGui::EndChild();
  ImGui::End();
}

void visit_instrument() {
  if (!_song) {
    return;
  }
  auto &ins = _song->instruments[_gui_instrument];
  ImGui::Begin("Instrument");
  if (ImGui::Button("Generate")) {
    std::lock_guard<std::mutex> guard{ _player->mutex() };
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

std::array<int, 12> key_rgb = { 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 1 };

void visit_pattern() {
  if (!_song) {
    return;
  }
  auto &pat = _song->patterns[_gui_pattern];
  ImGui::Begin("Pattern");

  ImGui::BeginChild("Hello There");

  const ImGuiStyle& Style = ImGui::GetStyle();
  const ImGuiIO& IO = ImGui::GetIO();
  ImDrawList* Draw = ImGui::GetWindowDrawList();

  const ImVec2 pos = ImGui::GetCursorScreenPos();
  const ImVec2 area = ImGui::GetContentRegionAvail();

  float size = 8.f;

  const float areax = std::min(area.x, 64.f * size);
  const float areay = std::min(area.y, 128 * size);

  const float minx = pos.x;
  const float maxx = pos.x + areax;
  const float miny = pos.y;
  const float maxy = pos.y + areay;

  int i = 0;
  for (float y = miny; y <= maxy; y += size, ++i) {
    int k = (11 - ((i + 1) % 12));
    uint32_t rgb = (k == 0) ? 0x80808080 : (key_rgb[k] ? 0x80404040 : 0x80808080);
    Draw->AddRectFilled(ImVec2{ minx, y-3 }, ImVec2{ maxx, y+3 }, rgb);
  }

  i = 0;
  for (float x = minx; x <= maxx; x += size, ++i) {
    uint32_t rgb = (i & 7) ? 0x40ffffff : 0x80ffffff;
    Draw->AddLine(ImVec2{ x, miny }, ImVec2{ x, maxy }, rgb);
  }

  {
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const int32_t ix = int32_t((IO.MousePos.x - minx + (size / 2)) / size);
    const int32_t iy = int32_t((IO.MousePos.y - miny + (size / 2)) / size);
    ImVec2 p = ImVec2{ minx + float(ix) * size, miny + float(iy) * size };
    Draw->AddCircle(p, size / 2.f, 0xff335577);
  }

  for (int i = 0; i < pat.notes_head; ++i) {
    const auto &note = pat.notes[i];
    const float x = note.start * size * 4.f;
    const float y = float(127 - (note.note)) * size;

    uint32_t rgb = (note.instrument == _gui_instrument) ? 0xffffffff : 0xffff8866;

    Draw->AddCircle(ImVec2{ minx + x, miny + y }, size / 2.f, rgb);
  }

  if (IO.MouseClicked[0] || IO.MouseClicked[1]) {
    const int32_t dx = int32_t((IO.MousePos.x - minx + (size / 2)) / size);
    const int32_t dy = int32_t((IO.MousePos.y - miny + (size / 2)) / size);

    Tracker::note_t n;
    n.instrument = _gui_instrument;
    n.start = float(dx) / 4.f;
    n.note = 127 - dy;

    if (n.start >= 0.f && n.start < 16.f && n.note > 0 && n.note <= 127) {
      std::lock_guard<std::mutex>(_player->mutex());
      if (IO.MouseClicked[0]) {
        pat.note_insert(n);
      }
      if (IO.MouseClicked[1]) {
        pat.note_remove(n);
      }
    }
  }

  ImGui::EndChild();
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
  visit_samples();
}

int main() {
  SDL_SetMainReady();
  if (!app_init()) {
    return -1;
  }
  if (!imgui_init()) {
    return -1;
  }

  load_samples();

  _song.reset(new Tracker::song_t);
  {
#if 0
    auto &pat = _song->patterns[0];
    pat.note_insert(Tracker::note_t{ 0,  69 + 12, 0 });
    pat.note_insert(Tracker::note_t{ 8,  69,      0 });
    pat.note_insert(Tracker::note_t{ 12, 69,      0 });
    pat.note_insert(Tracker::note_t{ 4,  69,      0 });
#endif
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
