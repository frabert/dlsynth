#include "Error.hpp"
#include "Instrument.hpp"
#include "NumericUtils.hpp"
#include "Sound.hpp"
#include "Synth/Synthesizer.hpp"
#include "Wave.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <dlsynth.h>
#include <limits>
#include <memory>
#include <riffcpp.hpp>
static std::atomic<int> dlsynth_error{DLSYNTH_NO_ERROR};

int dlsynth_get_last_error() { return dlsynth_error; }
void dlsynth_reset_last_error() { dlsynth_error = DLSYNTH_NO_ERROR; }

static void set_err_from_exception(riffcpp::ErrorType type) {
  switch (type) {
  case riffcpp::ErrorType::CannotOpenFile:
    dlsynth_error = DLSYNTH_CANNOT_OPEN_FILE;
    break;
  case riffcpp::ErrorType::InvalidFile:
    dlsynth_error = DLSYNTH_INVALID_FILE;
    break;
  case riffcpp::ErrorType::NullBuffer:
    dlsynth_error = DLSYNTH_INVALID_ARGS;
    break;
  default:
    dlsynth_error = DLSYNTH_UNKNOWN_ERROR;
    break;
  }
}

struct dlsynth {
  static constexpr std::size_t renderBufferSizeFrames = 1024;
  static constexpr std::size_t renderBufferSize = renderBufferSizeFrames * 2;

  std::unique_ptr<DLSynth::Synth::Synthesizer> synth;

  std::array<float, renderBufferSize> renderBuffer;

  ~dlsynth() { synth = nullptr; }
};

struct dlsynth_sound {
  DLSynth::Sound sound;
};
struct dlsynth_instr {
  const dlsynth_sound *sound;
  std::size_t index;
};

struct dlsynth_wav {
  DLSynth::Wave wave;
};
int dlsynth_load_sound_file(const char *path, uint32_t sampleRate,
                            dlsynth_sound **sound) {
  if (sound == nullptr || path == nullptr) {
    dlsynth_error = DLSYNTH_INVALID_ARGS;
    return 0;
  }

  try {
    riffcpp::Chunk chunk(path);
    *sound = new dlsynth_sound{DLSynth::Sound::readChunk(chunk, sampleRate)};

    return 1;
  } catch (riffcpp::Error &e) {
    set_err_from_exception(e.type());
    return 0;
  } catch (DLSynth::Error &e) {
    dlsynth_error = static_cast<int>(e.code());
    return 0;
  } catch (std::exception &) {
    dlsynth_error = DLSYNTH_UNKNOWN_ERROR;
    return 0;
  }
}

int dlsynth_load_sound_buf(const void *buf, size_t buf_size,
                           uint32_t sampleRate, dlsynth_sound **sound) {
  if (sound == nullptr || buf == nullptr || buf_size == 0) {
    dlsynth_error = DLSYNTH_INVALID_ARGS;
    return 0;
  }

  try {
    riffcpp::Chunk chunk(buf, buf_size);
    *sound = new dlsynth_sound{DLSynth::Sound::readChunk(chunk, sampleRate)};

    return 1;
  } catch (riffcpp::Error &e) {
    set_err_from_exception(e.type());
    return 0;
  } catch (DLSynth::Error &e) {
    dlsynth_error = static_cast<int>(e.code());
    return 0;
  } catch (std::exception &) {
    dlsynth_error = DLSYNTH_UNKNOWN_ERROR;
    return 0;
  }
}

int dlsynth_free_sound(dlsynth_sound *sound) {
  if (sound != nullptr) {
    delete sound;
  }

  return 1;
}

int dlsynth_sound_instr_count(const dlsynth_sound *sound, std::size_t *count) {
  if (sound == nullptr || count == nullptr) {
    dlsynth_error = DLSYNTH_INVALID_ARGS;
    return 0;
  }

  *count = sound->sound.instruments().size();
  return 1;
}

int dlsynth_sound_instr_info(const dlsynth_instr *instr, uint32_t *bank,
                             uint32_t *patch, int *isDrum) {
  if (instr == nullptr || bank == nullptr || patch == nullptr ||
      instr->sound == nullptr) {
    dlsynth_error = DLSYNTH_INVALID_ARGS;
    return 0;
  }

  const auto &i = instr->sound->sound.instruments()[instr->index];
  *bank = i.midiBank();
  *patch = i.midiInstrument();
  *isDrum = i.isDrumInstrument() ? 1 : 0;

  return 1;
}

int dlsynth_get_instr_patch(std::uint32_t bank, std::uint32_t patch, int drum,
                            const dlsynth_sound *sound, dlsynth_instr **instr) {
  if (sound == nullptr || instr == nullptr) {
    dlsynth_error = DLSYNTH_INVALID_ARGS;
    return 0;
  }

  const auto &instruments = sound->sound.instruments();
  bool isDrum = drum ? true : false;
  for (std::size_t i = 0; i < instruments.size(); i++) {
    const auto &instrument = instruments[i];
    if (instrument.midiBank() == bank && instrument.midiInstrument() == patch &&
        instrument.isDrumInstrument() == isDrum) {
      *instr = new dlsynth_instr();
      (*instr)->index = i;
      (*instr)->sound = sound;

      return 1;
    }
  }

  dlsynth_error = DLSYNTH_INVALID_INSTR;
  return 0;
}

int dlsynth_get_instr_num(std::size_t instr_num, const dlsynth_sound *sound,
                          dlsynth_instr **instr) {
  if (sound == nullptr || instr == nullptr) {
    dlsynth_error = DLSYNTH_INVALID_ARGS;
    return 0;
  }

  if (instr_num >= sound->sound.instruments().size()) {
    dlsynth_error = DLSYNTH_INVALID_INSTR;
    return 0;
  }

  *instr = new dlsynth_instr();
  (*instr)->index = instr_num;
  (*instr)->sound = sound;

  return 1;
}

int dlsynth_free_instr(dlsynth_instr *instr) {
  if (instr != nullptr) {
    delete instr;
  }

  return 1;
}

int dlsynth_load_wav_file(const char *path, dlsynth_wav **wav) {
  if (path == nullptr || wav == nullptr) {
    dlsynth_error = DLSYNTH_INVALID_ARGS;
    return 0;
  }

  try {
    riffcpp::Chunk chunk(path);
    *wav = new dlsynth_wav{DLSynth::Wave::readChunk(chunk)};

    return 1;
  } catch (riffcpp::Error &e) {
    set_err_from_exception(e.type());
    return 0;
  } catch (DLSynth::Error &e) {
    dlsynth_error = static_cast<int>(e.code());
    return 0;
  } catch (std::exception &) {
    dlsynth_error = DLSYNTH_UNKNOWN_ERROR;
    return 0;
  }
}

int dlsynth_load_wav_buffer(const void *buf, size_t buf_size,
                            dlsynth_wav **wav) {
  if (buf == nullptr || buf_size == 0 || wav == nullptr) {
    dlsynth_error = DLSYNTH_INVALID_ARGS;
    return 0;
  }

  try {
    riffcpp::Chunk chunk(buf, buf_size);
    *wav = new dlsynth_wav{DLSynth::Wave::readChunk(chunk)};

    return 1;
  } catch (riffcpp::Error &e) {
    set_err_from_exception(e.type());
    return 0;
  } catch (DLSynth::Error &e) {
    dlsynth_error = static_cast<int>(e.code());
    return 0;
  } catch (std::exception &) {
    dlsynth_error = DLSYNTH_UNKNOWN_ERROR;
    return 0;
  }
}

int dlsynth_get_wav_info(const dlsynth_wav *wav, int *sample_rate,
                         size_t *n_frames) {
  if (wav == nullptr || sample_rate == nullptr || n_frames == nullptr) {
    dlsynth_error = DLSYNTH_INVALID_ARGS;
    return 0;
  }

  *sample_rate = wav->wave.sampleRate();
  *n_frames = wav->wave.leftData().size();

  return 1;
}

int dlsynth_get_wav_data(const dlsynth_wav *wav, const float **left_buf,
                         const float **right_buf) {
  if (wav == nullptr || left_buf == nullptr || right_buf == nullptr) {
    dlsynth_error = DLSYNTH_INVALID_ARGS;
    return 0;
  }

  *left_buf = wav->wave.leftData().data();
  *right_buf = wav->wave.rightData().data();

  return 1;
}

int dlsynth_free_wav(struct dlsynth_wav *wav) {
  if (wav != nullptr) {
    delete wav;
  }

  return 1;
}

int dlsynth_init(int sample_rate, int num_voices, dlsynth **synth) {
  if (sample_rate <= 0 || num_voices <= 0 || synth == nullptr) {
    dlsynth_error = DLSYNTH_INVALID_ARGS;
    return 0;
  }

  *synth = new dlsynth();
  (*synth)->synth =
   std::make_unique<DLSynth::Synth::Synthesizer>(num_voices, sample_rate);

  return 1;
}

int dlsynth_free(dlsynth *synth) {
  if (synth != nullptr) {
    delete synth;
  }

  return 1;
}

int dlsynth_render_float(dlsynth *synth, size_t frames, float *lout,
                         float *rout, size_t incr, float gain) {
  if (synth == nullptr || lout == nullptr || incr == 0) {
    dlsynth_error = DLSYNTH_INVALID_ARGS;
    return 0;
  }

  float *lbuf = lout;
  float *lbuf_end = lbuf + (frames * incr);

  float *rbuf = rout;
  float *rbuf_end = rbuf + (frames * incr);

  synth->synth->render_fill(lbuf, lbuf_end, rbuf, rbuf_end, incr, gain);

  return 1;
}

int dlsynth_render_int16(dlsynth *synth, size_t frames, int16_t *lout,
                         int16_t *rout, size_t incr, float gain) {
  if (synth == nullptr || lout == nullptr || incr == 0) {
    dlsynth_error = DLSYNTH_INVALID_ARGS;
    return 0;
  }

  std::size_t remainingFrames = frames;
  const std::size_t framesInBuffer = dlsynth::renderBufferSize / 2;
  int16_t *l = lout;
  int16_t *r = rout;
  while (remainingFrames > 0) {
    std::size_t num_frames = std::min(remainingFrames, framesInBuffer);
    float *renderBuffer = synth->renderBuffer.data();
    dlsynth_render_float(synth, num_frames, renderBuffer, renderBuffer + 1, 2,
                         gain);

    for (size_t i = 0; i < num_frames; i++) {
      *l = DLSynth::inverse_normalize<std::int16_t>(renderBuffer[i * 2 + 0]);
      l += incr;

      if (r != nullptr) {
        *r = DLSynth::inverse_normalize<std::int16_t>(renderBuffer[i * 2 + 1]);
        r += incr;
      }
    }

    remainingFrames -= num_frames;
  }

  return 1;
}

int dlsynth_render_float_mix(dlsynth *synth, size_t frames, float *lout,
                             float *rout, size_t incr, float gain) {
  if (synth == nullptr || lout == nullptr || incr == 0) {
    dlsynth_error = DLSYNTH_INVALID_ARGS;
    return 0;
  }

  float *lbuf = lout;
  float *lbuf_end = lbuf + (frames * incr);

  float *rbuf = rout;
  float *rbuf_end = rbuf + (frames * incr);

  synth->synth->render_mix(lbuf, lbuf_end, rbuf, rbuf_end, incr, gain);

  return 1;
}

#define DLSYNTH_CHECK_SYNTH_NOT_NULL                                           \
  if (synth == nullptr || synth->synth == nullptr) {                           \
    dlsynth_error = DLSYNTH_INVALID_ARGS;                                      \
    return 0;                                                                  \
  }

int dlsynth_note_on(struct dlsynth *synth, const struct dlsynth_instr *instr,
                    int channel, int priority, uint8_t note, uint8_t velocity) {
  DLSYNTH_CHECK_SYNTH_NOT_NULL

  synth->synth->noteOn(instr->sound->sound, instr->index, channel, priority,
                       note, velocity);
  return 1;
}

int dlsynth_note_off(struct dlsynth *synth, int channel, uint8_t note) {
  DLSYNTH_CHECK_SYNTH_NOT_NULL

  synth->synth->noteOff(channel, note);
  return 1;
}

int dlsynth_poly_pressure(struct dlsynth *synth, int channel, uint8_t note,
                          uint8_t velocity) {
  DLSYNTH_CHECK_SYNTH_NOT_NULL

  synth->synth->pressure(channel, note, velocity);
  return 1;
}
int dlsynth_channel_pressure(struct dlsynth *synth, int channel,
                             uint8_t velocity) {
  DLSYNTH_CHECK_SYNTH_NOT_NULL

  synth->synth->pressure(channel, velocity);
  return 1;
}
int dlsynth_pitch_bend(struct dlsynth *synth, int channel, uint16_t value) {
  DLSYNTH_CHECK_SYNTH_NOT_NULL

  synth->synth->pitchBend(channel, value);
  return 1;
}
int dlsynth_volume(struct dlsynth *synth, int channel, uint8_t value) {
  DLSYNTH_CHECK_SYNTH_NOT_NULL

  synth->synth->volume(channel, value);
  return 1;
}
int dlsynth_pan(struct dlsynth *synth, int channel, uint8_t value) {
  DLSYNTH_CHECK_SYNTH_NOT_NULL

  synth->synth->pan(channel, value);
  return 1;
}
int dlsynth_modulation(struct dlsynth *synth, int channel, uint8_t value) {
  DLSYNTH_CHECK_SYNTH_NOT_NULL

  synth->synth->modulation(channel, value);
  return 1;
}
int dlsynth_sustain(struct dlsynth *synth, int channel, int status) {
  DLSYNTH_CHECK_SYNTH_NOT_NULL

  synth->synth->sustain(channel, status);
  return 1;
}
int dlsynth_reverb(struct dlsynth *synth, int channel, uint8_t value) {
  DLSYNTH_CHECK_SYNTH_NOT_NULL

  synth->synth->reverb(channel, value);
  return 1;
}
int dlsynth_chorus(struct dlsynth *synth, int channel, uint8_t value) {
  DLSYNTH_CHECK_SYNTH_NOT_NULL

  synth->synth->chorus(channel, value);
  return 1;
}
int dlsynth_pitch_bend_range(struct dlsynth *synth, int channel,
                             uint16_t value) {
  DLSYNTH_CHECK_SYNTH_NOT_NULL

  synth->synth->pitchBendRange(channel, value);
  return 1;
}
int dlsynth_fine_tuning(struct dlsynth *synth, int channel, uint16_t value) {
  DLSYNTH_CHECK_SYNTH_NOT_NULL

  synth->synth->fineTuning(channel, value);
  return 1;
}
int dlsynth_coarse_tuning(struct dlsynth *synth, int channel, uint16_t value) {
  DLSYNTH_CHECK_SYNTH_NOT_NULL

  synth->synth->coarseTuning(channel, value);
  return 1;
}
int dlsynth_reset_controllers(struct dlsynth *synth, int channel) {
  DLSYNTH_CHECK_SYNTH_NOT_NULL

  synth->synth->resetControllers(channel);
  return 1;
}
int dlsynth_all_notes_off(struct dlsynth *synth) {
  DLSYNTH_CHECK_SYNTH_NOT_NULL

  synth->synth->allNotesOff();
  return 1;
}
int dlsynth_all_sound_off(struct dlsynth *synth) {
  DLSYNTH_CHECK_SYNTH_NOT_NULL

  synth->synth->allSoundOff();
  return 1;
}

struct dlsynth_wavesample {
  DLSynth::Wavesample wavesample;
};

int dlsynth_new_wavesample_oneshot(uint16_t unityNote, int16_t fineTune,
                                   int32_t gain,
                                   struct dlsynth_wavesample **wavesample) {
  if (wavesample == nullptr) {
    dlsynth_error = DLSYNTH_INVALID_ARGS;
    return 0;
  }
  *wavesample =
   new dlsynth_wavesample{DLSynth::Wavesample(unityNote, fineTune, gain)};
  return 1;
}
int dlsynth_new_wavesample_looped(uint16_t unityNote, int16_t fineTune,
                                  int32_t gain, enum dlsynth_loop_type type,
                                  uint32_t loopStart, uint32_t loopLength,
                                  struct dlsynth_wavesample **wavesample) {
  if (wavesample == nullptr) {
    dlsynth_error = DLSYNTH_INVALID_ARGS;
    return 0;
  }
  *wavesample = new dlsynth_wavesample{DLSynth::Wavesample(
   unityNote, fineTune, gain,
   DLSynth::WavesampleLoop(static_cast<DLSynth::LoopType>(type), loopStart,
                           loopLength))};
  return 1;
}
int dlsynth_free_wavesample(struct dlsynth_wavesample *wavesample) {
  if (wavesample != nullptr) {
    delete wavesample;
  }

  return 1;
}

int dlsynth_new_wav_mono(int sampleRate, const float *dataBegin,
                         const float *dataEnd,
                         const struct dlsynth_wavesample *wavesample,
                         struct dlsynth_wav **wav) {
  if (wav == nullptr) {
    dlsynth_error = DLSYNTH_INVALID_ARGS;
    return 0;
  }

  std::vector<float> data(dataBegin, dataEnd);

  std::size_t dataSize = data.size();
  if (wavesample != nullptr) {
    if (wavesample->wavesample.loop() != nullptr) {
      auto loop = wavesample->wavesample.loop();
      if (loop->start() > dataSize ||
          loop->start() + loop->length() > dataSize) {
        dlsynth_error = DLSYNTH_INVALID_ARGS;
        return 0;
      }
    }
    *wav = new dlsynth_wav{DLSynth::Wave(
     data, data, sampleRate, &wavesample->wavesample, nullptr, nullptr)};
    return 1;
  } else {
    *wav = new dlsynth_wav{DLSynth::Wave(
     data, data, sampleRate, &wavesample->wavesample, nullptr, nullptr)};
    return 1;
  }
}
int dlsynth_new_wav_stereo(int sampleRate, const float *leftDataBegin,
                           const float *leftDataEnd,
                           const float *rightDataBegin,
                           const float *rightDataEnd,
                           const struct dlsynth_wavesample *wavesample,
                           struct dlsynth_wav **wav) {
  if (wav == nullptr) {
    dlsynth_error = DLSYNTH_INVALID_ARGS;
    return 0;
  }

  std::vector<float> ldata(leftDataBegin, leftDataEnd);
  std::vector<float> rdata(rightDataBegin, rightDataEnd);

  if (ldata.size() != rdata.size()) {
    dlsynth_error = DLSYNTH_INVALID_ARGS;
    return 0;
  }

  std::size_t dataSize = ldata.size();
  if (wavesample != nullptr) {
    if (wavesample->wavesample.loop() != nullptr) {
      auto loop = wavesample->wavesample.loop();
      if (loop->start() > dataSize ||
          loop->start() + loop->length() > dataSize) {
        dlsynth_error = DLSYNTH_INVALID_ARGS;
        return 0;
      }
    }
    *wav = new dlsynth_wav{DLSynth::Wave(
     ldata, rdata, sampleRate, &wavesample->wavesample, nullptr, nullptr)};
    return 1;
  } else {
    *wav = new dlsynth_wav{DLSynth::Wave(
     ldata, rdata, sampleRate, &wavesample->wavesample, nullptr, nullptr)};
    return 1;
  }
}

struct dlsynth_wavepool {
  std::vector<DLSynth::Wave> waves;
};

int dlsynth_new_wavepool(struct dlsynth_wavepool **wavepool) {
  if (wavepool == nullptr) {
    dlsynth_error = DLSYNTH_INVALID_ARGS;
    return 0;
  }

  *wavepool = new dlsynth_wavepool();
  return 1;
}
int dlsynth_wavepool_add(struct dlsynth_wavepool *wavepool,
                         const struct dlsynth_wav *wav) {
  if (wavepool == nullptr || wav == nullptr) {
    dlsynth_error = DLSYNTH_INVALID_ARGS;
    return 0;
  }

  wavepool->waves.push_back(wav->wave);
  return 1;
}

int dlsynth_free_wavepool(struct dlsynth_wavepool *wavepool) {
  if (wavepool != nullptr) {
    delete wavepool;
  }

  return 1;
}

struct dlsynth_blocklist {
  std::vector<DLSynth::ConnectionBlock> blocks;
};

int dlsynth_new_blocklist(struct dlsynth_blocklist **blocklist) {
  if (blocklist == nullptr) {
    dlsynth_error = DLSYNTH_INVALID_ARGS;
    return 0;
  }

  *blocklist = new dlsynth_blocklist();
  return 1;
}

int dlsynth_blocklist_add(
 struct dlsynth_blocklist *blocklist, enum dlsynth_source source,
 enum dlsynth_source control, enum dlsynth_dest destination, int32_t scale,
 int sourceInvert, int sourceBipolar, enum dlsynth_transf sourceTransform,
 int controlInvert, int controlBipolar, enum dlsynth_transf controlTransform) {
  if (blocklist == nullptr) {
    dlsynth_error = DLSYNTH_INVALID_ARGS;
    return 0;
  }

  blocklist->blocks.emplace_back(
   static_cast<DLSynth::Source>(source), static_cast<DLSynth::Source>(control),
   static_cast<DLSynth::Destination>(destination), scale,
   DLSynth::TransformParams(
    sourceInvert, sourceBipolar,
    static_cast<DLSynth::TransformType>(sourceTransform)),
   DLSynth::TransformParams(
    controlInvert, controlBipolar,
    static_cast<DLSynth::TransformType>(controlTransform)));

  return 1;
}

int dlsynth_free_blocklist(struct dlsynth_blocklist *blocklist) {
  if (blocklist != nullptr) {
    delete blocklist;
  }

  return 1;
}

struct dlsynth_regionlist {
  std::vector<DLSynth::Region> regions;
};

int dlsynth_new_regionlist(struct dlsynth_regionlist **regionlist) {
  if (regionlist == nullptr) {
    dlsynth_error = DLSYNTH_INVALID_ARGS;
    return 0;
  }

  *regionlist = new dlsynth_regionlist();
  return 1;
}

int dlsynth_add_region(struct dlsynth_regionlist *list, uint16_t minKey,
                       uint16_t maxKey, uint16_t minVelocity,
                       uint16_t maxVelocity,
                       const struct dlsynth_blocklist *blocklist,
                       uint32_t waveIndex, int selfNonExclusive) {
  if (list == nullptr || minKey > maxKey || minVelocity > maxVelocity ||
      blocklist == nullptr) {
    dlsynth_error = DLSYNTH_INVALID_ARGS;
    return 0;
  }

  list->regions.emplace_back(DLSynth::Range{minKey, maxKey},
                             DLSynth::Range{minVelocity, maxVelocity},
                             blocklist->blocks, waveIndex, selfNonExclusive);
  return 1;
}

int dlsynth_add_region_wavesample(struct dlsynth_regionlist *list,
                                  uint16_t minKey, uint16_t maxKey,
                                  uint16_t minVelocity, uint16_t maxVelocity,
                                  const struct dlsynth_blocklist *blocklist,
                                  uint32_t waveIndex, int selfNonExclusive,
                                  const struct dlsynth_wavesample *wavesample) {
  if (list == nullptr || minKey > maxKey || minVelocity > maxVelocity ||
      blocklist == nullptr) {
    dlsynth_error = DLSYNTH_INVALID_ARGS;
    return 0;
  }

  list->regions.emplace_back(
   DLSynth::Range{minKey, maxKey}, DLSynth::Range{minVelocity, maxVelocity},
   blocklist->blocks, waveIndex, selfNonExclusive, wavesample->wavesample);
  return 1;
}

int dlsynth_free_regionlist(struct dlsynth_regionlist *list) {
  if (list != nullptr) {
    delete list;
  }

  return 1;
}

struct dlsynth_instrlist {
  std::vector<DLSynth::Instrument> instruments;
};

int dlsynth_new_instrlist(struct dlsynth_instrlist **list) {
  if (list == nullptr) {
    dlsynth_error = DLSYNTH_INVALID_ARGS;
    return 0;
  }

  *list = new dlsynth_instrlist();
  return 1;
}

int dlsynth_add_instrument(struct dlsynth_instrlist *list, uint32_t midiBank,
                           uint32_t midiInstrument, int isDrumInstrument,
                           const struct dlsynth_blocklist *blocklist,
                           const struct dlsynth_regionlist *regions) {
  if (list == nullptr || blocklist == nullptr || regions == nullptr) {
    dlsynth_error = DLSYNTH_INVALID_ARGS;
    return 0;
  }

  list->instruments.emplace_back(midiBank, midiInstrument, isDrumInstrument,
                                 blocklist->blocks, regions->regions);
  return 1;
}

int dlsynth_free_instrlist(struct dlsynth_instrlist *list) {
  if (list != nullptr) {
    delete list;
  }

  return 1;
}

int dlsynth_new_sound(struct dlsynth_sound **sound,
                      const struct dlsynth_instrlist *instruments,
                      const struct dlsynth_wavepool *wavepool) {
  if (sound == nullptr || instruments == nullptr || wavepool == nullptr) {
    dlsynth_error = DLSYNTH_INVALID_ARGS;
    return 0;
  }

  auto wavepool_size = wavepool->waves.size();

  for (const auto &instr : instruments->instruments) {
    for (const auto &region : instr.regions()) {
      if (region.waveIndex() >= wavepool_size) {
        dlsynth_error = DLSYNTH_INVALID_WAVE_INDEX;
        return 0;
      }

      auto wsmpl = region.wavesample();
      const auto &wave = wavepool->waves[region.waveIndex()];
      if (!wsmpl) {
        wsmpl = wave.wavesample();
      }

      if (!wsmpl) {
        dlsynth_error = DLSYNTH_NO_WAVESAMPLE;
        return 0;
      }

      const DLSynth::WavesampleLoop *loop = wsmpl->loop();
      if (loop && (loop->start() >= wave.leftData().size() ||
                   loop->start() + loop->length() >= wave.leftData().size())) {
        dlsynth_error = DLSYNTH_INVALID_WAVESAMPLE;
        return 0;
      }
    }
  }

  *sound =
   new dlsynth_sound{DLSynth::Sound(instruments->instruments, wavepool->waves)};
  return 1;
}

#ifdef DLSYNTH_COMMIT
static dlsynth_version version = {DLSYNTH_MAJOR, DLSYNTH_MINOR, DLSYNTH_PATCH,
                                  DLSYNTH_COMMIT};
#else
static dlsynth_version version = {DLSYNTH_MAJOR, DLSYNTH_MINOR, DLSYNTH_PATCH,
                                  ""};
#endif

const dlsynth_version *dlsynth_get_version() { return &version; }