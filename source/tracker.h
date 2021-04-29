#pragma
#include <cstdint>
#include <memory>
#include <array>
#include <mutex>


namespace Tracker {

struct sample_t;
struct instrument_t;
struct note_t;
struct pattern_t;
struct song_t;
struct playing_note_t;
struct player_t;

enum {
  MAX_INSTUMENTS = 16,
  MAX_PATTERNS = 16,
  MAX_NOTES = 256,
  MAX_NOTES_PLAYING = 8,
  BEATS_IN_PATTERN = 16,
};

// position is actually the number of beats since the pattern start
typedef float position_t;

struct sample_t {

  sample_t()
    : size(0)
    , sample_rate(1)
  {
  }

  // number of samples
  uint32_t size;
  // sample rate
  uint32_t sample_rate;
  // sample data
  std::unique_ptr<int16_t[]> data;
};

struct instrument_t {

  instrument_t()
    : root(69 /* A4 */)
    , fine(0.f)
    , sample_start(0)
    , sample_end(0)
  {
  }

  // root semitone
  uint8_t root;
  float fine;
  // sample loop markes
  uint32_t sample_start;
  uint32_t sample_end;
  // sample data
  sample_t sample;
};

struct note_t {

  note_t()
    : start(0)
    , note(69 /* A4 */)
    , instrument(0)
  {}

  note_t(position_t start, uint8_t note, uint8_t instrument)
    : start(start)
    , note(note)
    , instrument(instrument)
  {
  }

  // note start position
  position_t start;
  // note semitone
  uint8_t note;
  // instrument index
  uint8_t instrument;
};

struct pattern_t {

  pattern_t()
    : notes_head(0)
  {
  }

  void note_insert(const note_t &n);

  void note_remove(const note_t &n);

  // notes in the pattern sorted by start time
  uint8_t notes_head;
  std::array<note_t, MAX_NOTES> notes;
};

struct song_t {

  song_t()
    : bpm(120)
  {}

  uint8_t bpm;

  std::array<instrument_t, MAX_INSTUMENTS> instruments;
  std::array<pattern_t, MAX_PATTERNS> patterns;
};

struct playing_note_t {

  playing_note_t()
    : instrument(0)
    , step(0.f)
    , position(0)
  {
  }

  // instrument index
  uint8_t instrument;
  // instrument sample step per output sample
  float step;
  // instrument sample position
  position_t position;

  void _trigger(const player_t &player, const note_t &note);

  // render a number of samples and return true if the sample
  // has now finished, otherwise false
  bool _render_samples(const player_t &player, int16_t *out, uint32_t samples);
};

struct player_t {

  player_t(const song_t &song, uint32_t sample_rate)
    : _song(song)
    , _pattern(song.patterns.data())
    , _playing(false)
    , _playback_pos{0}
    , _note(nullptr)
    , _sample_rate(sample_rate)
  {
  }

  void render(int16_t *out, uint32_t samples);

  void stop();
  void play();

  void set_pattern(uint32_t index);

  // play a new note immediately
  void play_note(const note_t &n);

  // return the player mutex
  std::mutex &mutex() {
    return _mutex;
  }

protected:
  friend struct playing_note_t;

  // find the next note
  // if n==nullptr return the first note in a pattern
  // if return==nullptr there is no next pattern
  const note_t *_next_note(const note_t *n);

  // reached a note
  void _on_note(const note_t *note);
  // end of pattern
  void _on_pattern_end();

  // try to render the requested number of samples but return
  // the number actually rendered
  uint32_t _render_samples(int16_t *out, uint32_t samples);

  const song_t &_song;
  const pattern_t *_pattern;

  // true if playing, false if not
  bool _playing;

  // pattern playback position
  position_t _playback_pos;
  // most recent note being played
  const note_t *_note;

  // output sample rate
  const uint32_t _sample_rate;
  // currently playing note stack
  std::array<playing_note_t, MAX_NOTES_PLAYING> _note_stack;

  // render thread mutex
  std::mutex _mutex;
};
}  // namespace Tracker
