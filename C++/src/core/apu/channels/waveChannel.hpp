#include "../../../utils/types.hpp"

class WaveChannel {
 private:
  byte waveTable[16] = {0};
  word timerLoad = 0;
  byte timer = 0;
  bool dacEnabled = 0;
  byte volumeCode = 0;
  byte outputVol = 0;
  bool lengthEnable = 0;
  bool triggerBit = 0;
  bool enabled = 0;
  byte lengthCounter = 0;
  void trigger();
  byte positionCounter = 0;

 public:
  WaveChannel();
  ~WaveChannel();
  void lengthClock();
  void step();
  byte getOutPutVol() const;
  byte readRegs(word address) const;
  void writeRegs(word address, byte data);
  bool getRunning() const;
};
