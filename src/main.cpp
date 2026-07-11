#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "Platform/Platform.h"
#include "core/GameBoy.hpp"

static void dumpScreenASCII(const GameBoy& gameboy) {
  const byte* data = gameboy.GetScreenData();
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

int main(int argc, char* argv[]) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <rom_path> [--test]\n", argv[0]);
    return 1;
  }

  bool testMode = false;
  for (int i = 2; i < argc; i++) {
    if (strcmp(argv[i], "--test") == 0)
      testMode = true;
  }

  GameBoy gameboy;
  gameboy.ReadRom(argv[1]);

  if (testMode) {
    fprintf(stderr, "Running in headless test mode...\n");
    fprintf(stderr, "ROM header: entry=");
    for (int i = 0x100; i < 0x104; i++)
      fprintf(stderr, "%02X ", gameboy.ReadMemory(i));
    fprintf(stderr, "\n");
    bool restarted = false;
    for (int frame = 0; frame < 36000 && !restarted; frame++) {
      int cyclesThisFrame = 0;
      while (cyclesThisFrame < 70224) {
        cyclesThisFrame += gameboy.Update();
        if (gameboy.IsRestartDetected()) {
          fprintf(stderr, "Stopping due to restart detection at frame %d.\n",
                  frame);
          restarted = true;
          break;
        }
      }
      if (!restarted && (frame < 60 || frame % 1000 == 0)) {
        fprintf(stderr, "Frame %d: SP=0x%04X PC=0x%04X IF=0x%02X IE=0x%02X\n",
                frame, gameboy.GetSP(), gameboy.GetPC(),
                gameboy.ReadMemory(0xFF0F), gameboy.ReadMemory(0xFFFF));
      }
    }
    fprintf(stderr, "Done. Screen output:\n\n");
    dumpScreenASCII(gameboy);

    fprintf(stderr, "\n--- Serial Output ---\n%s\n", gameboy.GetSerialOutput());
    return 0;
  }

  Platform platform("GameBoy", 1024, 768, 160, 144);

  bool quit = false;
  while (!quit) {
    quit = platform.ProcessInput(gameboy);
    int cyclesThisFrame = 0;
    while (cyclesThisFrame < 70224) {
      cyclesThisFrame += gameboy.Update();
    }
    platform.Update(gameboy.GetScreenData(), 160 * 4);
  }

  return 0;
}
