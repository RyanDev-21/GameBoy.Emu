#include <cstdio>
#include <cstring>

#include "Platform/Platform.h"
#include "core/GameBoy.hpp"

int main(int argc, char* argv[]) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <rom_path> [--test]\n", argv[0]);
    return 1;
  }

  GameBoy gameboy;
  gameboy.ReadRom(argv[1]);

  bool testMode = false;
  // skip the exec && romPath
  for (int i = 2; i < argc; i++) {
    if (strcmp(argv[i], "--test") == 0)
      testMode = true;
  }

  if (testMode) {
    gameboy.RunTestMode(36000);
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
