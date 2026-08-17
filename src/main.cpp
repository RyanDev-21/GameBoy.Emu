#include <cstdio>
// #include <cstdlib>
// #include <cstring>
#include <filesystem>
#include <stdexcept>

#include "Platform/Platform.h"
#include "core/GameBoy.hpp"

namespace fs = std::filesystem;
int countWhitePixel(const byte* screen) {
  int n = 0;
  for (int i = 0; i < 160 * 144; i++) {
    if (screen[i] && screen[i + 1] && screen[i + 2] && screen[i + 3]) {
      n++;
      screen += 4;
    }
  }
  return n;
}
int main(int argc, char* argv[]) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <rom_path> \n", argv[0]);
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
  // bool testMode = false;
  // skip the exec && romPath
  // for (int i = 2; i < argc; i++) {
  //   if (strcmp(argv[i], "--test") == 0)
  //     testMode = true;
  // }
  //
  // if (testMode) {
  //   gameboy.RunTestMode(36000);
  //   return 0;
  // }

  Platform platform("GameBoy", 1024, 768, 160, 144);

  bool quit = false;
  bool wasWhite = false;
  while (!quit) {
    quit = platform.ProcessInput(gameboy);
    int cyclesThisFrame = 0;
    int frame = 0;
    while (cyclesThisFrame < 70224) {
      if (cyclesThisFrame == 70224) {
        frame++;
      }
      cyclesThisFrame += gameboy.Update();
    }
    platform.Update(gameboy.GetScreenData(), 160 * 4);

    // These commented out lines are for when i was debugging
    // int white = countWhitePixel(gameboy.GetScreenData());
    // bool isWhite =
    //     (white >
    //      0.95f * 160 * 144);  // is 95 percent white or not of this frame
    // if (isWhite & !wasWhite) {
    //   fprintf(stderr,
    //           "White detect at "
    //           "frame:%d,whitePx=%d,pc=%04x,ly=%02x,lcdc=%02x\n",
    //           frame, white, gameboy.m_programCounter,
    //           gameboy.ReadMemory(0xFF44), gameboy.ReadMemory(0xFF40));
    // }

    // if (getenv("GB_DEBUG")) {
    //   fprintf(stderr, "\nBG_Paletter Value:\n");
    //   const word* palette_data = gameboy.getBG_Palette();
    //   for (int i = 0; i < 32; i++) {
    //     fprintf(stderr, "%04x", palette_data[i]);
    //   }
    //   fprintf(stderr,
    //   "\nWY=%02x,WX=%02x,VBK=%02x,pc=%04x,ly=%02x,lcdc=%02x\n",
    //           gameboy.ReadMemory(0xFF4A), gameboy.ReadMemory(0xFF4B),
    //
    //           gameboy.ReadMemory(0xFF4F), gameboy.m_programCounter,
    //           gameboy.ReadMemory(0xFF44), gameboy.ReadMemory(0xFF40));
    //   delete palette_data;
    //   const byte* vramData = gameboy.GetVram(0);
    //   fprintf(stderr, "\nWinMap:\n");
    //   for (int i = 0; i < 16; i++) {
    //     fprintf(stderr, "%02x", vramData[0x1C00 + i]);
    //   }
    //   fprintf(stderr, "\n");
    // }
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
