#include "squareChannel.hpp"

byte SquareChannel::readReg(byte address) const {
  byte currReg = (address & 0xF) & 0x5;
  byte returnData = 0;
  switch (currReg) {
    case 0x0:
      returnData = (sweepPeriodLoad << 4) | (negate << 3) | (sweepShift & 0x7);
    case 0x1: {
      byte lengthLoad = (lengthCounter - 64) & 0x3F;
      returnData = ((dutyCycle & 0x3) << 6) | lengthLoad;
    }; break;
    case 0x2: {
      returnData = ((volume & 0xF) << 4) | ((envAddMode & 0x01) << 3) |
                   (timerLoad & 0x7);
    }
    case 0x3: {
      returnData = timerLoad & 0xFF;
    }
    case 0x4: {
      returnData |= (triggerbit << 7);
      returnData |= (timerLoad & 0x700) >> 8;
      returnData |= (lengthEnable << 6);
    }
  }
  return returnData;
};

void SquareChannel::writeRegs(word address, byte data) {
  // module this as this can't be greater than the available regs
  byte currReg = (address & 0xF) & 0x5;
  switch (currReg) {
    case 0x0: {
      sweepShift = (data & 0x7);
      negate = ((data >> 3) & 0x01) != 0;
      sweepPeriodLoad = (data >> 4) & 0x7;
    } break;
    case 0x1: {
      byte lengthLoad = data & 0x3F;
      lengthCounter = (64 - lengthLoad);
      dutyCycle = (data >> 6) & 0x3;
    } break;
    case 0x2: {
      volume = (data >> 4) & 0xF;
      envAddMode = ((data >> 3) & 0x1) != 0;
      timerLoad = (data & 0x7);
    } break;
    case 0x3: {
      // LSB
      timerLoad = (timerLoad & 0x7FF) | data;
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
  if (lengthCounter > 0 && lengthEnable) {
    lengthCounter--;
  } else {
    enabled = false;
  }
}

void SquareChannel::Step() {
  if (--timer <= 0) {
    timer = (2047 - timerLoad) * 4;
    sequencePointer = (sequencePointer + 1) & 0x1F;
  }
  if (enabled) {
    outputVol = volume;
  } else {
    outputVol = 0;
  }
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
    new_feq = new_feq + shadowFeq;
  } else {
    new_feq = shadowFeq - new_feq;
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
    if (envRunning && envPeriod > 0) {
      if (envAddMode) {
        volume++;
      } else {
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
  if (lengthCounter == 0) {
    lengthCounter = 64;
  }
  envRunning = true;
  envPeriod = envPeriodLoad;
  volume = volumeLoad;
  timer = (2047 - timerLoad) * 4;
  shadowFeq = timerLoad;
  sweepPeriod = sweepPeriodLoad;
  if (sweepPeriod == 0) {
    sweepPeriod = 8;
  }
  enabled = sweepPeriod > 0 | sweepShift > 0;
  if (sweepShift > 0) {
    sweepCalc();
  }
}
