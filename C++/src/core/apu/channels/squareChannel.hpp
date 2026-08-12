#include "../../../utils/types.hpp"
class SquareChannel {
 private:
  // Square channel
  byte sweepShift = 0;
  bool negate = 0;
  byte sweepPeriod = 0;
  byte sweepPeriodLoad = 0;
  byte dutyCycle = 0;
  byte lengthCounter = 0;
  byte volume = 0;
  byte volumeLoad = 0;
  bool envAddMode = 0;
  bool envRunning = 0;
  byte envPeriod = 0;
  byte envPeriodLoad = 0;
  byte timerLoad = 0;
  word timer = 0;
  bool triggerbit = 0;
  bool enabled = 0;
  bool lengthEnable = 0;
  byte lengthLoad = 0;
  word shadowFeq = 0;
  bool sequencePointer = 0;
  byte outputVol = 0;
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
  byte readReg(word address) const;
  bool getEnvRunning() const;
};
