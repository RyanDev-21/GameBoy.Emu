#include "squareChannel.hpp"

#include <cstdio>

SquareChannel::SquareChannel() {
}
SquareChannel::~SquareChannel() {
}

byte SquareChannel::readReg(word address) const {
  byte currReg = (address & 0xF) % 0x5;
  byte returnData = 0;
  switch (currReg) {
    case 0x0:
      returnData = (sweepPeriodLoad << 4) | (negate << 3) | (sweepShift & 0x7);
      break;
    case 0x1: {
      returnData = ((dutyCycle & 0x3) << 6) | lengthLoad & 0x3F;
    }; break;
    case 0x2: {
      returnData =
          (volumeLoad << 4) | ((envAddMode & 0x01) << 3) | (timerLoad & 0x7);
    } break;
    case 0x3: {
      returnData = timerLoad & 0xFF;
    } break;
    case 0x4: {
      returnData |= (triggerbit << 7);
      returnData |= (timerLoad & 0x700) >> 8;
      returnData |= (lengthEnable << 6);
    } break;
  }
  return returnData;
};

void SquareChannel::writeRegs(word address, byte data) {
  // module this as this can't be greater than the available regs
  byte currReg = (address & 0xF) % 0x5;
  switch (currReg) {
    case 0x0: {
      sweepShift = (data & 0x7);
      negate = ((data >> 3) & 0x01) != 0;
      sweepPeriodLoad = (data >> 4) & 0x7;
    } break;
    case 0x1: {
      lengthLoad = data & 0x3F;
      // as the length bits combined value is 64 and rather than counting upward
      // we just subtract from the max value and see if it is max or not
      lengthCounter = (64 - lengthLoad);
      dutyCycle = (data >> 6) & 0x3;
    } break;
    case 0x2: {
      volumeLoad = (data >> 4) & 0xF;
      envAddMode = ((data >> 3) & 0x1) != 0;
      dacEnabled = (data & 0xF8) != 0;
      if (!dacEnabled) {
        enabled = false;
      }
      envPeriodLoad = (data & 0x7);
      envPeriod = envPeriodLoad;
      volume = volumeLoad;
    } break;
    case 0x3: {
      // LSB
      // frequency/timer is 11 bit long so have to take three bit  value
      // left of 11bit and then merge with new as the new one is going to
      // be a 8 bit it is just overwrite its lsb
      timerLoad = (timerLoad & 0x700) | data;
    } break;
    case 0x4: {
      lengthEnable = ((data >> 6) & 0x01) != 0;
      // MSB
      word msbTimer = (data & 0x07);
      timerLoad = (msbTimer << 8) | (timerLoad & 0xFF);
      triggerbit = ((data >> 7) & 0x01) != 0;
      if (triggerbit) {
        Trigger();
      }
    } break;
  }
}

void SquareChannel::lengthClock() {
  if (lengthEnable) {
    if (lengthCounter > 0) {
      lengthCounter--;
    }
    if (lengthCounter == 0) {
      enabled = false;
    }
  }
}

void SquareChannel::Step() {
  if (--timer <= 0) {
    // 4 dots per cycle
    timer = (2048 - timerLoad) * 4;
    sequencePointer = (sequencePointer + 1) & 0x07;
  }
  if (enabled && dacEnabled) {
    outputVol = volume;
  } else {
    outputVol = 0;
  }
  // this doesn't make too much diff on the volume so for the case of
  // exiting i just ignore it
  if (!dutyTable[dutyCycle][sequencePointer]) {
    outputVol = 0;
  }
}
byte SquareChannel::GetOutPutVol() {
  return outputVol;
}

void SquareChannel::sweepClock() {
  if (--sweepPeriod <= 0) {
    sweepPeriod = sweepPeriodLoad;
    if (sweepPeriod == 0) {
      sweepPeriod = 8;
    }
    word newFeq = sweepCalc();
    if (newFeq <= 2047 && sweepShift > 0) {
      shadowFeq = newFeq;
      timerLoad = newFeq;
      sweepCalc();
    }
    sweepCalc();
  }
}
word SquareChannel::sweepCalc() {
  word new_feq = shadowFeq >> sweepShift;
  if (negate) {
    new_feq = new_feq - shadowFeq;
  } else {
    new_feq = shadowFeq + new_feq;
  }
  if (new_feq > 2047) {
    enabled = false;
  }
  return new_feq;
}

void SquareChannel::EnvClock() {
  if (--envPeriod <= 0) {
    envPeriod = envPeriodLoad;
    if (envPeriod == 0) {
      envPeriod = 8;
    }
    if (envRunning && envPeriodLoad > 0) {
      if (envAddMode && volume < 15) {
        volume++;
      } else if (!envAddMode && volume > 0) {
        volume--;
      }
    }
    if (volume == 0 || volume == 15) {
      envRunning = false;
    }
  }
}

void SquareChannel::Trigger() {
  enabled = true;
  timer = (2048 - timerLoad) * 4;
  if (lengthCounter == 0) {
    lengthCounter = 64;
  }
  envRunning = true;
  envPeriod = envPeriodLoad;
  volume = volumeLoad;
  shadowFeq = timerLoad;
  sweepPeriod = sweepPeriodLoad;
  if (sweepPeriod == 0) {
    sweepPeriod = 8;
  }
  // Do not override `enabled` here — triggering should enable the channel
  // regardless of sweep configuration. The sweep may later disable the
  // channel if it overflows during sweepCalc().
  if (sweepShift > 0) {
    sweepCalc();
  }
}

bool SquareChannel::getEnvRunning() const {
  return envRunning;
}

bool SquareChannel::getRunning() const {
  return lengthCounter > 0;
}

void SquareChannel::DebugPrint() const {
  fprintf(stderr, "Square Chanel\n");
  fprintf(stderr, "**Timer** \ntimer:%d\n", timer);
  fprintf(stderr, "TimerLoad:%d\n", timerLoad);
  fprintf(stderr, "shadowFeq:%d\n", shadowFeq);
  fprintf(stderr, "**Enabled And stuff** \ndacEnabled:%d\n", dacEnabled);
  fprintf(stderr, "enabled:%d\n", enabled);
  fprintf(stderr, "triggerBit:%d\n", triggerbit);
  fprintf(stderr, "lengthEnabled:%d\n", lengthEnable);
  fprintf(stderr, "**Volume**volume:%d\n", volume);
  fprintf(stderr, "volume load :%d\n", volumeLoad);
  fprintf(stderr, "outputVol:%d\n", outputVol);
  fprintf(stderr, "**Envelop stuff**\n envAddMode:%d\n", envAddMode);
  fprintf(stderr, "envelopPeriod:%d\n", envPeriod);
  fprintf(stderr, "envelopPeriodLoad:%d\n", envPeriodLoad);
  fprintf(stderr, "envelopRunning:%d\n", envRunning);
  fprintf(stderr, "**Sweep**\nSweepPeriod:%d\n", sweepPeriod);
  fprintf(stderr, "SweepPeriodLoad:%d\n", sweepPeriodLoad);
  fprintf(stderr, "sweepShift:%d\n", sweepShift);
  fprintf(stderr, "negate:%d\n", negate);
  fprintf(stderr, "sequencePointer:%d\n", sequencePointer);
}
