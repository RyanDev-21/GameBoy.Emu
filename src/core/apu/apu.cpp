#include "apu.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>
#include <SDL2/SDL_timer.h>

APU::APU() {
  SDL_Init(SDL_INIT_AUDIO);
  SDL_AudioSpec spec = {0};
  // spec.freq = 44100;
  spec.freq = 44100;
  spec.format = AUDIO_F32SYS;
  spec.channels = 2;
  spec.samples = sample_size;
  spec.callback = NULL;
  spec.userdata = this;
  device_id = SDL_OpenAudioDevice(NULL, 0, &spec, NULL, 0);
  if (device_id == 0) {
    fprintf(stderr, "SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
  }
  SDL_PauseAudioDevice(device_id, 0);
}

APU::~APU() {
}

void APU::step(int cycles) {
  while (cycles-- != 0) {
    if (--frameSequencerCount <= 0) {
      //  Ticks every 8192 CPU cycles (on a 512 scale, 4194304/512 = 8192).
      frameSequencerCount = 8192;
      switch (frameSequnce) {
        case 0:
          square_1.lengthClock();
          square_2.lengthClock();
          wave.lengthClock();
          noise.lengthClock();
          break;
        case 1: break;
        case 2:
          square_1.sweepClock();
          square_1.lengthClock();
          square_2.lengthClock();
          wave.lengthClock();
          noise.lengthClock();
          break;
        case 3: break;
        case 4:
          square_1.lengthClock();
          square_2.lengthClock();
          wave.lengthClock();
          noise.lengthClock();
          break;
        case 5: break;
        case 6:
          square_1.sweepClock();
          square_1.lengthClock();
          square_2.lengthClock();
          wave.lengthClock();
          noise.lengthClock();
          break;
        case 7:
          square_1.EnvClock();
          square_2.EnvClock();
          noise.envClock();
          break;
      }
      if (++frameSequnce >= 8) {
        frameSequnce = 0;
      }
    }
    square_1.Step();
    square_2.Step();
    wave.step();
    noise.step();

    if (--downSampleCounter <= 0) {
      downSampleCounter = 87;

      // Left
      float bufferin0 = 0;
      float bufferin1 = 0;
      int volume = (128 * leftVolume) /
                   7;  // Should approximate an integer for the mixer volume
      if (leftEnabled[0]) {
        bufferin1 = ((float)square_1.GetOutPutVol()) / 100;
        SDL_MixAudioFormat((Uint8*)&bufferin0, (Uint8*)&bufferin1, AUDIO_F32SYS,
                           sizeof(float), volume);
      }
      if (leftEnabled[1]) {
        bufferin1 = ((float)square_2.GetOutPutVol()) / 100;
        SDL_MixAudioFormat((Uint8*)&bufferin0, (Uint8*)&bufferin1, AUDIO_F32SYS,
                           sizeof(float), volume);
      }
      if (leftEnabled[2]) {
        bufferin1 = ((float)wave.getOutPutVol()) / 100;
        SDL_MixAudioFormat((Uint8*)&bufferin0, (Uint8*)&bufferin1, AUDIO_F32SYS,
                           sizeof(float), volume);
      }
      if (leftEnabled[3]) {
        bufferin1 = ((float)noise.getOutputVol()) / 100;
        SDL_MixAudioFormat((Uint8*)&bufferin0, (Uint8*)&bufferin1, AUDIO_F32SYS,
                           sizeof(float), volume);
      }
      mainBuffer[bufferFillAmount] = bufferin0;

      // Right
      bufferin0 = 0;
      volume = (128 * rightVolume) / 7;
      if (rightEnabled[0]) {
        bufferin1 = ((float)square_1.GetOutPutVol()) / 100;
        SDL_MixAudioFormat((Uint8*)&bufferin0, (Uint8*)&bufferin1, AUDIO_F32SYS,
                           sizeof(float), volume);
      }
      if (rightEnabled[1]) {
        bufferin1 = ((float)square_2.GetOutPutVol()) / 100;
        SDL_MixAudioFormat((Uint8*)&bufferin0, (Uint8*)&bufferin1, AUDIO_F32SYS,
                           sizeof(float), volume);
      }
      if (rightEnabled[2]) {
        bufferin1 = ((float)wave.getOutPutVol()) / 100;
        SDL_MixAudioFormat((Uint8*)&bufferin0, (Uint8*)&bufferin1, AUDIO_F32SYS,
                           sizeof(float), volume);
      }
      if (rightEnabled[3]) {
        bufferin1 = ((float)noise.getOutputVol()) / 100;
        SDL_MixAudioFormat((Uint8*)&bufferin0, (Uint8*)&bufferin1, AUDIO_F32SYS,
                           sizeof(float), volume);
      }

      // AudioHandler::playAudio();
      //  Section below off to play audio
      mainBuffer[bufferFillAmount + 1] = bufferin0;

      bufferFillAmount += 2;  // as sum is by 4 channel take average
      // but this is not necessary
      // left /= 4.0f;
      // right /= 4.0f;
      //
      // left = HighPassFilter(left, leftCapacitor);
      // right = HighPassFilter(right, rightCapacitor);

      // write interleaved stereo floats (two floats per frame)
      // mainBuffer[bufferFillAmount++] = left;
      // mainBuffer[bufferFillAmount++] = right;
      // full (bufferFillAmount counts floats, sample_size is frames)

      if (bufferFillAmount >= sample_size) {
        bufferFillAmount = 0;
        while (SDL_GetQueuedAudioSize(device_id) >
               sample_size * sizeof(float)) {
          SDL_Delay(1);
        }
        SDL_QueueAudio(device_id, mainBuffer, sample_size * sizeof(float));
      }
    }
  }
}

void APU::handleWriteRouting(word address, byte data) {
  byte reg = address & 0xFF;
  if (reg >= 0x10 && reg <= 0x14) {
    square_1.writeRegs(address, data);
  } else if (reg >= 0x16 && reg <= 0x19) {
    square_2.writeRegs(address, data);
  } else if (reg >= 0x1A && reg <= 0x1E) {
    wave.writeRegs(address, data);
  } else if (reg >= 0x30 && reg <= 0x3F) {
    wave.writeRegs(address, data);
  } else if (reg >= 0x20 && reg <= 0x23) {
    noise.writeRegs(address, data);
  } else if (reg >= 0x24 && reg <= 0x26) {
    switch (reg) {
      case 0x24:
        leftVinEnable = (data & 0x80) != 0;
        leftVolume = (data >> 4) & 0x7;
        rightVinEnable = (data & 0x8) != 0;
        rightVolume = data & 0x7;
        break;
      case 0x25:
        for (int i = 0; i < 4; i++) {
          rightEnabled[i] = (data >> i) & 0x01;
          leftEnabled[i] = (data >> (i + 4)) & 0x01;
        }
        break;
      case 0x26:
        if ((data & 0x80) != 0x80) {
          for (int i = 0xFF10; i < 0xFF26; i++) {
            handleWriteRouting(i, 0);
          }
          powerControl = false;
        } else if (!powerControl) {  // only turn on if the powerControl is
                                     // perviously off
          // this is weird
          frameSequnce = 0;
          // reset wave table
          for (int i = 0xFF30; i <= 0xFF3F; i++) {
            handleWriteRouting(i, 0);
          }
          powerControl = true;
        }

        break;
    }
  }
};
byte APU::handleReadRouting(word address) const {
  byte result = 0;
  byte reg = address & 0xFF;

  if (reg >= 0x10 && reg <= 0x14) {
    result |= square_1.readReg(address);
  } else if (reg >= 0x16 && reg <= 0x19) {
    result |= square_2.readReg(address);
  } else if (reg >= 0x1A && reg <= 0x1E) {
    result |= wave.readRegs(address);
  } else if (reg >= 0x30 && reg <= 0x3F) {
    result |= wave.readRegs(address);
  } else if (reg >= 0x20 && reg <= 0x23) {
    result |= noise.readRegs(address);
  } else if (reg >= 0x24 && reg <= 0x26) {
    switch (reg) {
      case 0x24:
        result |= (leftVinEnable) << 7 | (leftVolume & 0x07) << 4 |
                  (rightVinEnable) << 3 | (rightVolume & 0x07);
        break;
      case 0x25:
        for (int i = 0; i < 4; i++) {
          result |= leftEnabled[i] << (i + 4);
          result |= rightEnabled[i] << i;
        }
        break;
      case 0x26:
        // This has to return  all the channel and main one
        result |= powerControl << 7;
        result |= square_1.getRunning();
        result |= square_2.getRunning() << 1;
        result |= wave.getRunning() << 2;
        result |= noise.getRunning() << 3;
        break;
    }
  }
  if (reg <= 0x26) {
    result |= readOrValues[reg - 0x10];
  }
  return result;
};

// float APU::HighPassFilter(float in, float& capacitor) {
//   float out = in - capacitor;
//   capacitor = in - out * kCharge;
//   return out;
// }
//
// void APU::DebugPrint() const {
//   for (int i = 0; i < sample_size; i++) {
//     fprintf(stderr, "sample value at index %d : %f\n", i, mainBuffer[i]);
//   }
// }
