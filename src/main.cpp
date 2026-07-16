#include <cstdio>
#include <cstring>
#include <filesystem>
#include <stdexcept>

#include "Platform/Platform.h"
#include "core/GameBoy.hpp"

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <rom_path> [--test]\n", argv[0]);
    return 1;
  }
  const char* romName = argv[1];
  std::string dirName = "saveFiles";
  fs::path dirPath = fs::path(dirName);
  fs::path romPath = fs::path(romName);
  std::string stem = romPath.stem().string();
  std::string extension = ".sav";
  std::string filePath = (dirPath / (stem + extension)).string();
  GameBoy gameboy;
  gameboy.ReadRom(romName);
  gameboy.LoadRam(filePath.c_str());

  // for test mode
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

  try {
    if (fs::create_directories(dirPath)) {
      fprintf(stdout, "Created the directory successfully\n");
    } else {
      fprintf(stdout, "Skipped: Directory already exists\n");
    }
    gameboy.SaveRam(filePath.c_str());
  } catch (const fs::filesystem_error& e) {
    fprintf(stderr, "File SystemError: %s\n", e.what());
  } catch (const std::runtime_error& e) {
    fprintf(stderr, "GameBoy Error: %s\n", e.what());
  }

  return 0;
}
