#ifndef DEBUG_HPP
#define DEBUG_HPP

#include "../utils/types.hpp"

class GameBoy;

namespace Debug {
void Print(const char* fmt, ...);
void Error(const char* fmt, ...);
void DumpScreenASCII(const byte* screenData);
void PrintROMHeader(GameBoy& gb);
void PrintFrameInfo(int frame, word pc, word sp, byte ifReg, byte ieReg);
void PrintSerialOutput(const char* serial);
}  // namespace Debug

#endif
