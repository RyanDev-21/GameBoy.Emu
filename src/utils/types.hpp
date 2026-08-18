#ifndef TYPES_HPP
#define TYPES_HPP

#include <vector>
#define FLAG_Z 7
#define FLAG_N 6
#define FLAG_H 5
#define FLAG_C 4
#define KEY_1 0xFF4D
#include <chrono>

using Clock = std::chrono::system_clock;
using TimePoint = Clock::time_point;
enum COLOUR {
  WHITE = 0,
  LIGHT_GRAY = 1,
  DARK_GRAY = 2,
  BLACK = 3,
};
typedef unsigned char byte;
typedef unsigned short word;
typedef signed short signed_word;
typedef signed char signed_byte;
union Register {
  word reg;
  struct {
    byte lo;
    byte hi;
  };
};

typedef struct {
  byte regs[5];
  byte latch[5];
  float RTCaccumulator;
  byte RTCWriteState;
  bool mbc3RTCreg;
} RTCregs;

typedef struct {
  bool masterInterrupt;
  bool EIpending;
  bool halt;
  bool halt_Bug;
  bool previousStatusLine;
} Interrupt;

typedef struct {
  bool hdmaActive;
  bool hdmaHBlankMode;
  bool hdmaLineDone;
  word hdmaRemaining;
} HDMA;

typedef struct {
  byte tima;
  byte tma;
  byte tmc;
  RTCregs rtc;
} timer;

typedef struct {
  byte current_ramBank;
  byte enabled_ram;
  byte enabled_rom;
  word current_romBank;
  byte current_vramBank;
  byte current_wramBank;
  bool MBC1;
  bool MBC2;
  bool MBC3;
  bool MBC5;
} MBC;

typedef struct {
  bool isGBC;
  word RegisterAF;
  word RegisterBC;
  word RegisterDE;
  word RegisterHL;
  word StackPointer;
  size_t ramSize;
  bool doubleSpeed;
} Internal;

struct GBCcolor {
  byte r, g, b;
};

enum CC {
  NZ = 0,
  Z = 1,
  NC = 2,
  C = 3,
};

enum OP {
  NONE = 1,
  INC = 2,
  DEC = 3,
};

enum DutyCycle {
  ZERO = 0,
  ONE = 1,
  TWO = 2,
  THREE = 3,
};

#pragma pack(push, 1)
struct saveData {
  std::vector<byte> ramBanks;
  byte RTCregs[5];
  TimePoint RTCTimeStamp;
};
#pragma pack(pop)

// For APU
struct channel1 {};

struct TileAttributes {
  byte palette : 3;   // BG palette 0-7
  byte vramBank : 1;  // which bank the tile DATA lives in
  byte unused : 1;
  byte hFlip : 1;
  byte vFlip : 1;
  byte priority : 1;  // BG-to-OAM priority
};
#endif
