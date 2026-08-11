#include "../../utils/types.hpp"
class SquareChannel {
 private:
  // Square channel
  byte sweepShift;
  bool negate;
  byte sweepPeriod;
  byte sweepPeriodLoad;
  byte dutyCycle;
  byte lengthCounter;
  byte volume;
  byte volumeLoad;
  bool envAddMode;
  bool envRunning;
  byte envPeriod;
  byte envPeriodLoad;
  byte timerLoad;
  word timer;
  bool triggerbit;
  bool enabled;
  bool lengthEnable;
  word shadowFeq;
  bool sequencePointer;
  byte outputVol;
  const bool dutyTable[4][8] = {
      {false, false, false, false, false, false, false, true},
      {true, false, false, false, false, false, false, true},
      {true, false, false, false, false, true, true, true},
      {false, true, true, true, true, true, true, false}};
  void Trigger();

 public:
  SquareChannel();
  ~SquareChannel();
  void writeRegs(word address, byte data);
  void Step();
  byte GetOutPutVol();
  void lengthClock();
  void sweepClock();
  void EnvClock();
  word sweepCalc();
  byte readReg(byte address) const;
};
