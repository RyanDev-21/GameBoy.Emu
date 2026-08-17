#include <SDL2/SDL_audio.h>

#include "../../utils/types.hpp"
#include "channels/noiseChannel.hpp"
#include "channels/squareChannel.hpp"
#include "channels/waveChannel.hpp"
// as this is the standard size based on the sdl
#define sample_size 4096

#pragma once
class APU {
 private:
  SquareChannel square_1;
  SquareChannel square_2;
  WaveChannel wave;
  NoiseChannel noise;
  bool rightVinEnable = 0;
  bool leftVinEnable = 0;
  byte leftEnabled[4] = {false};
  byte rightEnabled[4] = {false};
  byte leftVolume = 0;
  byte rightVolume = 0;
  bool powerControl = false;
  int downSampleCounter = 87;
  int bufferFillAmount = 0;
  SDL_AudioDeviceID device_id;
  // stereo interleaved buffer: `sample_size` frames × 2 channels
  float mainBuffer[sample_size] = {0};
  int frameSequencerCount = 0;
  byte frameSequnce = 0;
  const uint8_t readOrValues[23] = {
      0x80, 0x3f, 0x00, 0xff, 0xbf, 0xff, 0x3f, 0x00, 0xff, 0xbf, 0x7f, 0xff,
      0x9f, 0xff, 0xbf, 0xff, 0xff, 0x00, 0x00, 0xbf, 0x00, 0x00, 0x70};

 public:
  APU();
  ~APU();
  void step(int cycles);
  void handleWriteRouting(word address, byte data);
  byte handleReadRouting(word address) const;
  // void DebugPrint() const;
};
