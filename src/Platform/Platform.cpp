#include "Platform.h"

#include <SDL2/SDL_events.h>
#include <SDL2/SDL_keyboard.h>
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_scancode.h>

#include <cstdio>
#include <fstream>

#include "../utils/StringUtils.hpp"

Platform::Platform(char const* title, int windowWidth, int windowHeight,
                   int textureWidth, int textureHeight) {
  SDL_Init(SDL_INIT_VIDEO);
  window = SDL_CreateWindow(title, 0, 0, windowWidth, windowHeight,
                            SDL_WINDOW_SHOWN);
  renderer = SDL_CreateRenderer(
      window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888,
                              SDL_TEXTUREACCESS_STREAMING, textureWidth,
                              textureHeight);
  gbButtons = {
      {"RIGHT", 0}, {"LEFT", 1}, {"UP", 2},     {"DOWN", 3},
      {"A", 4},     {"B", 5},    {"SELECT", 6}, {"START", 7},
  };
  for (int i = 0; i < SDL_NUM_SCANCODES; i++) {
    keys[i] = -1;
  }
  std::ifstream file("gameboy_gamePad.config");
  SDL_Scancode default_keys[8] = {
      SDL_SCANCODE_D,  // RIGHT
      SDL_SCANCODE_A,  // LEFT
      SDL_SCANCODE_W,  // UP
      SDL_SCANCODE_S,  // DOWN
      SDL_SCANCODE_J,  // A
      SDL_SCANCODE_K,  // B
      SDL_SCANCODE_U,  // SELECT
      SDL_SCANCODE_I,  // START
  };
  if (!file.is_open()) {
    fprintf(stderr, "Could not open the config file.\nUsing Default config.\n");
    for (int i = 0; i < 8; i++) {
      keys[default_keys[i]] = i;
    }
  } else {
    processKeysFromFile(file);
  }
}

void Platform::Update(void const* buffer, int pitch) {
  SDL_UpdateTexture(texture, nullptr, buffer, pitch);
  SDL_RenderClear(renderer);
  SDL_RenderCopy(renderer, texture, nullptr, nullptr);
  SDL_RenderPresent(renderer);
}

bool Platform::ProcessInput(GameBoy& gameBoy) {
  bool quit = false;

  SDL_Event event;

  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_QUIT) {
      quit = true;
    }
    if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
      int btn = keys[event.key.keysym.scancode];
      if (btn >= 0) {
        switch (event.type) {
          case SDL_KEYDOWN: gameBoy.KeyPressed(btn); break;
          case SDL_KEYUP: gameBoy.KeyReleased(btn); break;
        }
      }
      if (event.key.keysym.sym == SDLK_ESCAPE) {
        quit = true;
      }
    }
  }

  return quit;
}

void Platform::processKeysFromFile(std::ifstream& file) {
  std::string line;
  while (std::getline(file, line, '\n')) {
    auto eq = line.find('=');
    std::string btn = StringUtils::trim(line.substr(0, eq));
    std::string key = StringUtils::trim(line.substr(eq + 1));
    int button = gbButtons[btn];
    SDL_Scancode sc = SDL_GetScancodeFromName(key.c_str());
    keys[sc] = button;
  }
}
