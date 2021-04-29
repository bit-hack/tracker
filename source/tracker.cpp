#include <cassert>
#include <mutex>

#include "tracker.h"

//  A4=69 (440hz)
//
//  120 BPM
//  2 BP/S
//
//  sample_step = (sample_rate_instrument / sample_rate_global) * note_to_rate(note)

namespace {

uint32_t beats_to_samples(uint32_t sample_rate, uint32_t bpm, float beats) {
  // beats per second
  const double bps = double(bpm) / 60.0;
  // beats to seconds
  const double seconds = double(beats) / bps;
  // seconds to samples
  return uint32_t(double(sample_rate) * seconds);
}

float samples_to_beats(uint32_t sample_rate, uint32_t bpm, uint32_t samples) {
  // beats per second
  const double bps = double(bpm) / 60.0;
  // samples to seconds
  const double seconds = double(samples) / double(sample_rate);
  // seconds to beats
  return float(seconds * bps);
}

float note_to_rate(float note, float root) {
  // where root is typicaly 69
  return powf(2.f, ((note - 69) + (root - 69)) / 12.f);
}

}  // namespace

namespace Tracker {

// number of beats in a pattern
static const position_t PAT_END_POS = float(BEATS_IN_PATTERN);

void pattern_t::note_insert(const note_t &n) {
  // find insertion point in array
  uint32_t i = 0;
  for (; i < notes_head; ++i) {
    if (n.start <= notes[i].start) {
      break;
    }
  }
  // move array to make space
  for (uint32_t j = notes_head; j > i; --j) {
    notes[j] = notes[j - 1];
  }
  // insert into the array
  notes[i] = n;
  ++notes_head;
}

void pattern_t::note_remove(const note_t &n) {
  bool found = false;
  uint8_t i = 0;
  for (i = 0; i < notes_head; ++i) {
    if (notes[i].start != n.start) {
      continue;
    }
    if (notes[i].instrument != n.instrument) {
      continue;
    }
    if (notes[i].note != n.note) {
      continue;
    }
    for (; i < notes_head; ++i) {
      notes[i] = notes[i + 1];
    }
    --notes_head;
    break;
  }
}

void playing_note_t::_trigger(const player_t &player, const note_t &note) {
  const song_t &song = player._song;
  const instrument_t &inst = song.instruments[note.instrument];
  instrument = note.instrument;
  position = position_t(inst.sample_start);
  step = (float(inst.sample.sample_rate) / float(player._sample_rate)) *
    note_to_rate(note.note + inst.fine, inst.root);
}

void player_t::stop() {
  std::lock_guard<std::mutex> guard{ _mutex };
  _playing = false;
  _playback_pos = 0;
  _note = nullptr;
}

void player_t::play() {
  std::lock_guard<std::mutex> guard{ _mutex };
  _playing = true;
  _playback_pos = 0;
  _note = nullptr;
}

void player_t::set_pattern(uint32_t index) {
  std::lock_guard<std::mutex> guard{ _mutex };
  assert(index < _song.patterns.size());
  _pattern = &_song.patterns[index];
  // erase the current note
  _note = nullptr;
}

void player_t::play_note(const note_t &note) {
  std::lock_guard<std::mutex> guard{ _mutex };
  // insert into the note stack
  for (auto &n : _note_stack) {
    if (n.step == 0.f) {
      n._trigger(*this, note);
      return;
    }
  }
}

const note_t *player_t::_next_note(const note_t *n) {
  assert(_pattern);
  // if the pattern is empty
  if (_pattern->notes_head == 0) {
    return nullptr;
  }
  // if input is nullptr return the first note of the pattern
  if (n == nullptr) {
    return _pattern->notes.data();
  }
  // beyond last note return nullptr to signal end of the pattern
  const note_t *last = _pattern->notes.data() + _pattern->notes_head;
  if ((n + 1) >= last) {
    return nullptr;
  }
  // return the next note
  return n + 1;
}

void player_t::_on_pattern_end() {
  // restart
  _playback_pos = 0;
  _note = nullptr;
}

void player_t::render(int16_t *out, uint32_t samples) {
  // grab a lock so that no-one can change our data while
  // we are using it
  if (_mutex.try_lock()) {
    if (_playing) {
      // repeat until all samples have been rendered
      while (samples) {
        uint32_t done = _render_samples(out, samples);
        samples -= done;
        out += done;
      }
    }
    _mutex.unlock();
  }
  else {
    // clear samples?
  }
}

uint32_t player_t::_render_samples(int16_t *out, uint32_t samples) {
  // get the next note
  const note_t *next = _next_note(_note);

  // time until next note
  auto diff = next ? (next->start - _playback_pos) :
                     (PAT_END_POS - _playback_pos);
  if (diff < 0.f) {
    diff = 0;
  }
  // number of samples until the next note
  const uint32_t max_samples = beats_to_samples(_sample_rate, _song.bpm, diff);
  // max samples we can render
  const uint32_t num_samples = std::min(samples, max_samples);
  // render each note in turn
  for (auto &n : _note_stack) {
    // skip notes that are not playing
    if (n.step == 0.f) {
      continue;
    }
    // render this instrument
    if (n._render_samples(*this, out, num_samples)) {
      // sample has finished
      n.step = 0.f;
    }
  }
  // update the playback position
  _playback_pos += samples_to_beats(_sample_rate, _song.bpm, num_samples);

  if (_playback_pos >= PAT_END_POS) {
    _playback_pos = PAT_END_POS;
  }

  // if we rendered enough to read the next note
  if (num_samples == max_samples) {
    if (next) {
      _on_note(next);
    }
    else {
      _on_pattern_end();
    }
  }
  // return the number of samples we rendered
  return num_samples;
}

bool playing_note_t::_render_samples(const player_t &player, int16_t *out, uint32_t samples) {
  if (step == 0.f) {
    return true;
  }
  const song_t &song = player._song;
  const instrument_t &inst = song.instruments[instrument];
  const int16_t *samp = inst.sample.data.get();
  for (uint32_t i = 0; i < samples; ++i) {
    const uint32_t p = uint32_t(position);
    if (p >= inst.sample_end) {
      // note has finished
      step = 0.f;
      return true;
    }
    // we mix with the output stream here
    out[i] += (int32_t(samp[p]) * 12) >> 8;
    // increment the playback position
    position += step;
  }
  return false;
}

void player_t::_on_note(const note_t *note) {
  // we have now reached this note
  _note = note;
  // 
  playing_note_t *out = nullptr;
  for (auto &n : _note_stack) {
    if (n.step == 0.f) {
      out = &n;
      break;
    }
  }
  if (out) {
    // trigger the new note
    out->_trigger(*this, *note);
  }
}

}  // namespace Tracker
