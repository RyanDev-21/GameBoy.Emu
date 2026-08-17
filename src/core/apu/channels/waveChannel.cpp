#include "waveChannel.hpp"

#include <cstdio>

WaveChannel::WaveChannel() {
}
WaveChannel::~WaveChannel() {
}
byte WaveChannel::readRegs(word address) const {
  byte regVal = address & 0xF;
  byte result = 0;
  if (address >= 0xFF1A && address <= 0xFF1E) {
    switch (regVal) {
      case 0xA:
        result |= dacEnabled << 7;
        break;
        // recheck this line again
      case 0xB: result |= 256 - lengthCounter; break;
      case 0xC: result |= ((volumeCode & 0x3) << 5); break;
      case 0xD: result |= timerLoad & 0xFF; break;
      case 0xE:
        result |=
            (triggerBit << 7) | (lengthEnable << 6) | (timerLoad & 0x700) >> 8;
        break;
    }
  } else if (address >= 0xFF30 && address <= 0xFF3F) {  // wave ram
    result = waveTable[regVal];
  }
  return result;
}
void WaveChannel::writeRegs(word address, byte data) {
  byte regVal = address & 0xF;
  if (address >= 0xFF1A && address <= 0xFF1E) {
    switch (regVal) {
      case 0xA: {
        dacEnabled = (data & 0x80) != 0;
        if (!dacEnabled) {
          enabled = false;
        }
      } break;
      case 0xB: {
        byte lengthLoad = data;
        lengthCounter = 256 - data;
      } break;
      case 0xC: volumeCode = (data >> 5) & 0x3; break;
      case 0xD:
        // LSB
        // same as SquareChannel
        timerLoad = (timerLoad & 0x700) | data;
        break;
      case 0xE:
        // MSB
        // same as squareChannel
        lengthEnable = (data & 0x40) != 0;
        timerLoad = (timerLoad & 0xFF) | ((data & 0x7) << 8);
        triggerBit = (data & 0x80) != 0;
        if (triggerBit) {
          trigger();
        }
    }
  } else if (address >= 0xFF30 && address <= 0xFF3F) {  // wave ram
    waveTable[regVal] = data;
  }
}

// the wave ram is  16 bytes long and each byte stores
// two samples each 4-bit
void WaveChannel::step() {
  if (--timer <= 0) {
    timer = (2048 - timerLoad) * 2;
    positionCounter = (positionCounter + 1) & 0x1F;
  }
  if (enabled && dacEnabled) {
    int position = positionCounter / 2;
    byte outputByte = waveTable[position];
    // select upper/lower nibble based on positionCounter parity
    if ((positionCounter & 1) != 0) {
      outputByte = outputByte & 0x0F;
    } else {
      outputByte = (outputByte >> 4) & 0x0F;
    }
    // The DAC receives the current value from the upper/lower nibble of the
    // sample buffer, shifted right by the volume control.(from gbdev.gg8.se)
    if (volumeCode > 0) {
      outputVol = outputByte >> (volumeCode - 1);
    } else {
      outputVol = 0;
    }
  } else {
    outputVol = 0;
  }
}

void WaveChannel::lengthClock() {
  if (lengthEnable) {
    if (lengthCounter > 0) {
      lengthCounter--;
    }
    if (lengthCounter == 0) {
      enabled = 0;
    }
  }
};

void WaveChannel::trigger() {
  enabled = true;
  if (lengthCounter == 0) {
    lengthCounter = 256;
  }
  // 2 dots per cycle
  timer = (2048 - timerLoad) * 2;
  positionCounter = 0;
}
byte WaveChannel::getOutPutVol() const {
  return outputVol;
}

bool WaveChannel::getRunning() const {
  return lengthCounter > 0;
}

void WaveChannel::DebugPrint() const {
  fprintf(stderr, "Wave Chanel\n");
  fprintf(stderr, "timerLoad :%d\n", timerLoad);
  fprintf(stderr, "timer:%d\n", timer);
  fprintf(stderr, "dacEnabled:%d\n", dacEnabled);
  fprintf(stderr, "volumeCode:%d\n", volumeCode);
  fprintf(stderr, "outputVol:%d\n", outputVol);
  fprintf(stderr, "lengthEnable:%d\n", lengthEnable);
  fprintf(stderr, "enabled:%d\n", enabled);
  fprintf(stderr, "positionCounter:%d\n", positionCounter);
  for (int i = 0; i < 16; i++) {
    fprintf(stderr, "waveTable value at index %d:%d\n", i, waveTable[i]);
  }
}
