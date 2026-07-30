#ifndef TYPES_HPP
#define TYPES_HPP

#define FLAG_Z 7
#define FLAG_N 6
#define FLAG_H 5
#define FLAG_C 4

#include <chrono>

using Clock = std::chrono::system_clock;
using TimePoint = Clock::time_point;
enum COLOUR {
  WHITE = 0,
  LIGHT_GRAY = 1,
  DARK_GRAY = 2,
  BLACK = 3,
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

#pragma pack(push, 1)
struct saveData {
  byte ramBanks[0x8000];
  byte RTCregs[5];
  TimePoint RTCTimeStamp;
};
#pragma pack(pop)

// For APU
struct channel1 {};

#endif
