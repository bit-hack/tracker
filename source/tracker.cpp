#include <assert.h>
#include "tracker.h"

namespace Tracker {

void pattern_t::note_insert(const note_t &n) {

}

void pattern_t::note_remove(const note_t &n) {
  // make sure note to remove is part of this pattern
  assert(&n >= &*notes.begin() && &n < &notes[notes_head]);
}

void player_t::stop() {
  std::lock_guard<std::mutex> guard{ _mutex };
}

void player_t::play() {
  std::lock_guard<std::mutex> guard{ _mutex };
}

void player_t::set_pattern(uint32_t index) {
  std::lock_guard<std::mutex> guard{ _mutex };
  assert(index < _song.patterns.size());
  _pattern = &_song.patterns[index];
}

void player_t::play_note(const note_t &n) {
}

const note_t *player_t::_next_note(const note_t *n) {
  assert(_pattern);
  
}

void player_t::render(int16_t *out, uint32_t samples) {
  while (samples) {
    samples -= _render_samples(out, samples);
  }
}

uint32_t player_t::_render_samples(int16_t *out, uint32_t samples) {
  // time until the next event
  const note_t *next = _next_note(_note);
  // number of samples to render
  uint32_t num_samples = 0; // todo

  // render each note in turn
  for (size_t i = 0; i < _note_head; ++i) {
    const playing_note_t &n = _note_stack[i];
  }

  // add any notes that need to be played

}

}  // namespace Tracker
