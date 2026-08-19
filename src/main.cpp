#include <cstdio>
// #include <cstdlib>
// #include <cstring>
#include <filesystem>
#include <stdexcept>

#include "Platform/Platform.h"
#include "core/GameBoy.hpp"

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
  // Headless smoke test mode for CI: run minimal frames and exit.
  if (argc == 2 &&
      (strcmp(argv[1], "--smoketest") == 0 || strcmp(argv[1], "--test") == 0)) {
    fprintf(stdout, "Running smoketest (headless)...\n");
    GameBoy gameboy;
    // run a few frames to exercise the core loop without creating SDL window
    for (int f = 0; f < 10; ++f) {
      int cyclesThisFrame = 0;
      int iterations = 0;
      while (cyclesThisFrame < 70244 && iterations < 100000) {
        cyclesThisFrame += gameboy.Update();
        iterations++;
      }
    }
    fprintf(stdout, "Smoketest complete\n");
    return 0;
  }

  if (argc < 3) {
    fprintf(stdout, "Usage: %s <rom_path> --default/--debug \n", argv[0]);
    return 1;
  }
  const char* romName = argv[1];
  const char* option = argv[2];
  bool debug = strcmp(option, "--default") != 0;
  // Blargg regression mode
  if (argc >= 2 && strcmp(option, "--blargg") == 0) {
    // usage: <exe> <rom> --blargg [max_frames]
    int maxFrames = 200000;
    if (argc >= 4)
      maxFrames = atoi(argv[3]);
    GameBoy gameboy;
    gameboy.ReadRom(romName);
    if (gameboy.getCartridgeMemory().empty()) {
      fprintf(stderr, "Failed to load ROM: %s\n", romName);
      return 3;
    }
    fprintf(stdout, "Running blargg test %s (maxFrames=%d)...\n", romName,
            maxFrames);
    bool passed = false;
    for (int frame = 0; frame < maxFrames; ++frame) {
      int cyclesThisFrame = 0;
      while (cyclesThisFrame < 70244)
        cyclesThisFrame += gameboy.Update();
      std::string serial = gameboy.GetSerialOutput();
      if (serial.find("Passed") != std::string::npos ||
          serial.find("passed") != std::string::npos) {
        passed = true;
        break;
      }
    }
    std::string finalSerial = gameboy.GetSerialOutput();
    fprintf(stdout, "Serial output:\n%s\n", finalSerial.c_str());
    return passed ? 0 : 2;
  }

  // Snapshot mode: <exe> <rom> --snapshot <frames> <out.ppm>
  if (argc >= 2 && strcmp(option, "--snapshot") == 0) {
    if (argc < 5) {
      fprintf(stderr, "Usage: %s <rom> --snapshot <frames> <out.ppm>\n",
              argv[0]);
      return 1;
    }
    int frames = atoi(argv[3]);
    const char* outFile = argv[4];
    GameBoy gameboy;
    gameboy.ReadRom(romName);
    if (gameboy.getCartridgeMemory().empty()) {
      fprintf(stderr, "Failed to load ROM: %s\n", romName);
      return 3;
    }
    for (int f = 0; f < frames; ++f) {
      int cyclesThisFrame = 0;
      while (cyclesThisFrame < 70244)
        cyclesThisFrame += gameboy.Update();
    }
    const byte* screen = gameboy.GetScreenData();
    FILE* f = fopen(outFile, "wb");
    if (!f) {
      fprintf(stderr, "Failed to open %s for writing\n", outFile);
      return 1;
    }
    fprintf(f, "P6\n160 144\n255\n");
    // screen is RGBA 4 bytes per pixel, write RGB
    for (int i = 0; i < 160 * 144; ++i) {
      const unsigned char* p = &screen[i * 4];
      unsigned char rgb[3] = {p[0], p[1], p[2]};
      fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    fprintf(stdout, "Wrote snapshot to %s\n", outFile);
    return 0;
  }
  std::string dirName = "saveFiles";
  fs::path dirPath = fs::path(dirName);
  fs::path romPath = fs::path(romName);
  std::string stem = romPath.stem().string();
  std::string extension = ".sav";
  std::string filePath = (dirPath / (stem + extension)).string();
  GameBoy gameboy;
  gameboy.ReadRom(romName);
  gameboy.LoadRam(filePath.c_str());

  Platform platform("GameBoy", 1024, 768, 160, 144);
  bool quit = false;
  while (!quit) {
    quit = platform.ProcessInput(gameboy);
    int cyclesThisFrame = 0;
    while (cyclesThisFrame < 70244) {
      cyclesThisFrame += gameboy.Update();
    }
    debug ? platform.UpdateWithDebug(gameboy.GetScreenData(), 160 * 4, &gameboy)
          : platform.Update(gameboy.GetScreenData(), 160 * 4);
  }

  try {
    if (fs::create_directories(dirPath)) {
      fprintf(stdout, "Created the directory successfully\n");
    } else {
      fprintf(stdout, "Skipped: Directory already exists\n");
    }
    if (gameboy.SaveRam(filePath.c_str()) != 1) {
      fprintf(stdout, "Failed to save the game ram on Exit\n");
    };
  } catch (const fs::filesystem_error& e) {
    fprintf(stderr, "File SystemError: %s\n", e.what());
  } catch (const std::runtime_error& e) {
    fprintf(stderr, "GameBoy Error: %s\n", e.what());
  }

  return 0;
}
