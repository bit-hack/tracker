#pragma
#include <cstdint>
#include <memory>
#include <array>
#include <mutex>


namespace Tracker {

enum {
  MAX_INSTUMENTS = 16,
  MAX_PATTERNS = 16,
  MAX_NOTES = 256,
  MAX_NOTES_PLAYING = 8,
};

struct position_t {
  float pos;
};

struct sample_t {
  // sample data
  std::unique_ptr<int16_t[]> data;
  // number of samples
  uint32_t size;
};

struct instrument_t {
  // root semitone  A4=69 (440hz)
  uint8_t root;
  float fine;
  // 0xff - full
  // 0x00 - mute
  uint8_t volume;
  // sample loop markes
  uint32_t sample_start;
  uint32_t sample_end;
  // 0 left, 127 centre, 255 right
  uint8_t pan;
  // loop sample
  bool looping;
  // sample data
  sample_t sample;
};

struct note_t {
  // note start and end positions
  position_t start;
  position_t end;
  // note semitone  A4=69 (440hz)
  uint8_t note;
  // instrument index
  uint8_t instrument;
};

struct pattern_t {

  void note_insert(const note_t &n);

  void note_remove(const note_t &n);

  // notes in the pattern sorted by start time
  std::array<note_t, MAX_NOTES> notes;
  uint8_t notes_head;
};

struct song_t {

  // bpm
  uint8_t tempo;

  std::array<instrument_t, MAX_INSTUMENTS> instruments;
  std::array<pattern_t, MAX_PATTERNS> patterns;
};

struct playing_note_t {
  // instrument index
  const sample_t *instrument;
  // instrument sample step per output sample
  float rate;
  // instrument sample position
  float position;
};

struct player_t {

  player_t(const song_t &song, uint32_t sample_rate)
    : _song(song)
    , _pattern(nullptr)
    , _playback_pos{0}
    , _sample_rate(sample_rate)
  {
  }

  void render(int16_t *out, uint32_t samples);

  void stop();
  void play();

  void set_pattern(uint32_t index);

  // play a new note immediately
  void play_note(const note_t &n);

protected:
  // find the next note
  // if n==nullptr return the first note in a pattern
  // if return==nullptr there is no next pattern
  const note_t *_next_note(const note_t *n);

  // try to render the requested number of samples but return
  // the number actually rendered
  uint32_t _render_samples(int16_t *out, uint32_t samples);

  const song_t &_song;
  const pattern_t *_pattern;

  // pattern playback position
  position_t _playback_pos;
  // most recent note being played
  const note_t *_note;

  // output sample rate
  const uint32_t _sample_rate;
  // currently playing note stack
  std::array<playing_note_t, MAX_NOTES_PLAYING> _note_stack;
  uint32_t _note_head;

  // render thread mutex
  std::mutex _mutex;
};
}  // namespace Tracker
