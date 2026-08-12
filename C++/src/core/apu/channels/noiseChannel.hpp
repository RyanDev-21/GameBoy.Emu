#include "../../../utils/types.hpp"

class NoiseChannel {
 private:
  byte envelopPeriodLoad = 0;
  byte envelopPeriod = 0;
  bool envelopRunning = 0;
  word timer = 0;
  byte lengthCounter = 0;
  byte volume = 0;
  byte outputVol = 0;
  byte volumeLoad = 0;
  bool envelopAddmode = 0;
  bool lengthEnabled = 0;
  bool triggerBit = 0;
  bool enabled = 0;
  bool dacEnabled = 0;
  byte clockShift = 0;
  bool lsfrWidth = 0;
  word lsfr = 0;
  byte dividerCode = 0;
  byte dividerTable[8] = {8, 16, 32, 48, 64, 80, 96, 112};

 public:
  NoiseChannel();
  ~NoiseChannel();
  void writeRegs(word address, byte data);
  byte readRegs(word address);
  void trigger();
  void step();
  void envClock();
  void lengthClock();
  bool getEnvRunning() const;
  byte getOutputVol() const;
};
