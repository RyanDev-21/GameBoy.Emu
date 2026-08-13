#include "noiseChannel.hpp"

#include <cstdio>

NoiseChannel::NoiseChannel() {
}
NoiseChannel::~NoiseChannel() {
}

byte NoiseChannel::readRegs(word address) const {
  byte currReg = (address & 0xF) % 0x05;
  byte result = 0;
  switch (currReg) {
    case 0x1: result |= (64 - lengthCounter) & 0x3F; break;
    case 0x2: {
      result |= (volumeLoad << 4) | (envelopAddmode & 0x01) << 3 |
                (envelopPeriod & 0x07);
    } break;
    case 0x3:
      result |= (clockShift & 0xF) << 4 | (lsfrWidth & 0x01) << 3 |
                (dividerCode & 0x07);
      break;
    case 0x4: result |= (triggerBit & 0x01) << 7 | (lengthEnabled) << 6; break;
  }

  return result;
}

void NoiseChannel::writeRegs(word address, byte data) {
  byte currReg = (address & 0xF) % 0x05;
  switch (currReg) {
    case 0x1: {
      byte lengthLoad = (data & 0x3F);
      lengthCounter = 64 - lengthLoad;
    } break;
    case 0x2: {
      dacEnabled = (data & 0xF8) != 0;
      volumeLoad = (data >> 4) & 0xF;
      envelopAddmode = (data & 0x08) != 0;
      envelopPeriodLoad = data & 0x7;
      envelopPeriod = envelopPeriodLoad;
      volume = volumeLoad;
    } break;
    case 0x3: {
      clockShift = data & 0xF0;
      lsfrWidth = (data & 0x08) != 0;
      dividerCode = data & 0x07;
    } break;
    case 0x4: {
      lengthEnabled = (data & 0x40) != 0;
      triggerBit = (data & 0x80) != 0;
      if (triggerBit) {
        trigger();
      }
    } break;
  }
};

void NoiseChannel::step() {
  if (--timer <= 0) {
    timer = dividerTable[dividerCode] << clockShift;  // this is so weird
    // bit 0 and bit 1 are xored to form 15bit
    byte extractBit = (lsfr & 0x01) ^ ((lsfr >> 1) & 0x01);
    lsfr |= extractBit << 14;
    // if this one is on then we need overwrite the 7 bit too
    if (lsfrWidth) {
      lsfr |= extractBit << 6;
    }
    lsfr >>= 1;
    if (enabled && dacEnabled && (lsfr & 0x01) == 0) {
      outputVol = volume;
    } else {
      outputVol = 0;
    }
  }
}

void NoiseChannel::envClock() {
  if (--envelopPeriod <= 0) {
    envelopPeriod = envelopPeriodLoad;
    if (envelopPeriod == 0) {
      envelopPeriod = 8;
    }
    if (envelopRunning && envelopPeriod > 0) {
      if (envelopAddmode && volume < 15) {
        volume++;
      } else if (!envelopAddmode && volume > 0) {
        volume--;
      }
    }
    if (volume == 15 || volume == 0) {
      envelopRunning = true;
    }
  }
}
void NoiseChannel::lengthClock() {
  if (lengthEnabled && lengthCounter > 0) {
    lengthCounter--;
  } else if (lengthCounter == 0) {
    enabled = false;
  }
}
void NoiseChannel::trigger() {
  enabled = true;
  if (lengthCounter == 0) {
    lengthCounter = 64;
  }

  // The frequency at which the LFSR is clocked is
  // 262144/divider×2^shift  Hz
  timer = dividerTable[dividerCode] << clockShift;
  envelopPeriod = envelopPeriodLoad;
  envelopRunning = true;
  volume = volumeLoad;

  lsfr = 0x7FFF;  // based on gbdev.gg8.se
}

byte NoiseChannel::getOutputVol() const {
  return outputVol;
}

bool NoiseChannel::getEnvRunning() const {
  return envelopRunning;
}

bool NoiseChannel::getRunning() const {
  return lengthCounter > 0;
}
