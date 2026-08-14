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
    case 0x0: result |= (64 - lengthCounter) & 0x3F; break;
    case 0x1: {
      result |= (volumeLoad << 4) | (envelopAddmode & 0x01) << 3 |
                (envelopPeriod & 0x07);
    } break;
    case 0x2:
      result |= (clockShift & 0xF) << 4 | (lsfrWidth & 0x01) << 3 |
                (dividerCode & 0x07);
      break;
    case 0x3: result |= (triggerBit & 0x01) << 7 | (lengthEnabled) << 6; break;
  }

  return result;
}

void NoiseChannel::writeRegs(word address, byte data) {
  byte currReg = (address & 0xF) % 0x05;
  switch (currReg) {
    case 0x0: {
      byte lengthLoad = (data & 0x3F);
      lengthCounter = 64 - lengthLoad;
    } break;
    case 0x1: {
      dacEnabled = (data & 0xF8) != 0;
      volumeLoad = (data >> 4) & 0xF;
      envelopAddmode = (data & 0x08) != 0;
      envelopPeriodLoad = data & 0x7;
      envelopPeriod = envelopPeriodLoad;
      volume = volumeLoad;
    } break;
    case 0x2: {
      clockShift = (data >> 4) & 0x0F;
      lsfrWidth = (data & 0x08) != 0;
      dividerCode = data & 0x07;
    } break;
    case 0x3: {
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
    // compute feedback from bit0 and bit1
    byte feedback = (lsfr & 0x01) ^ ((lsfr >> 1) & 0x01);
    lsfr >>= 1;
    lsfr |= (word)(feedback << 14);
    // if width mode (7-bit LFSR) also set bit 6 to feedback
    if (lsfrWidth) {
      // clear bit 6 then set it to feedback
      lsfr &= ~(1 << 6);
      lsfr |= (word)(feedback << 6);
    }
    // select output bit depending on width mode
    byte outBit = lsfr & 0x01;
    if (lsfrWidth)
      outBit = (lsfr >> 6) & 0x01;
    if (enabled && dacEnabled && outBit == 0) {
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
      envelopRunning = false;
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

void NoiseChannel::DebugPrint() const {
  fprintf(stderr, "Noise Chanel\n");
  fprintf(stderr, "**Timer** \ntimer:%d\n", timer);
  fprintf(stderr, "dividerCode:%d\n", dividerCode);
  fprintf(stderr, "**Enabled And stuff** \ndacEnabled:%d\n", dacEnabled);
  fprintf(stderr, "enabled:%d\n", enabled);
  fprintf(stderr, "triggerBit:%d\n", triggerBit);
  fprintf(stderr, "lengthEnabled:%d\n", lengthEnabled);
  fprintf(stderr, "**Volume**volume:%d\n", volume);
  fprintf(stderr, "volume load :%d\n", volumeLoad);
  fprintf(stderr, "outputVol:%d\n", outputVol);
  fprintf(stderr, "**Envelop stuff**\n envAddMode:%d\n", envelopAddmode);
  fprintf(stderr, "envelopPeriod:%d\n", envelopPeriod);
  fprintf(stderr, "envelopPeriodLoad:%d\n", envelopPeriodLoad);
  fprintf(stderr, "envelopRunning:%d\n", envelopRunning);
  fprintf(stderr, "**LSFR**\nlsfr:%d\n", lsfr);
  fprintf(stderr, "clockShift:%d\n", clockShift);
  fprintf(stderr, "lsfrWidth:%d\n", lsfrWidth);
}
