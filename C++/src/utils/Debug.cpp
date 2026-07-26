#include "Debug.hpp"

#include <cstdarg>
#include <cstdio>

#include "../core/GameBoy.hpp"

void Debug::Print(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
}

void Debug::Error(const char* fmt, ...) {
  fprintf(stderr, "Error: ");
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
}

void Debug::DumpScreenASCII(const byte* data) {
  for (int y = 0; y < 144; y++) {
    for (int x = 0; x < 160; x++) {
      int idx = (y * 160 + x) * 4;
      int r = data[idx + 0];
      int g = data[idx + 1];
      int b = data[idx + 2];
      int brightness = (r + g + b) / 3;
      if (brightness > 200)
        putchar(' ');
      else if (brightness > 150)
        putchar('.');
      else if (brightness > 80)
        putchar('O');
      else
        putchar('#');
    }
    putchar('\n');
  }
}

void Debug::PrintROMHeader(GameBoy& gb) {
  fprintf(stderr, "ROM header: entry=");
  for (int i = 0x100; i < 0x104; i++)
    fprintf(stderr, "%02X ", gb.ReadMemory(i));
  fprintf(stderr, "\n");
}

void Debug::PrintFrameInfo(int frame, word pc, word sp, byte ifReg,
                           byte ieReg) {
  fprintf(stderr, "Frame %d: SP=0x%04X PC=0x%04X IF=0x%02X IE=0x%02X\n", frame,
          sp, pc, ifReg, ieReg);
}

void Debug::PrintSerialOutput(const char* serial) {
  fprintf(stderr, "\n--- Serial Output ---\n%s\n", serial);
}
