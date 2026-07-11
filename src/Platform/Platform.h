#ifndef PLATFORM_H
#define PLATFORM_H
#include <SDL2/SDL.h>
#include <SDL2/SDL_scancode.h>

#include <fstream>
#include <iostream>
#include <map>

#include "../core/GameBoy.hpp"
class Platform {
 public:
  Platform(char const* title, int windowWidth, int windowHeight,
           int textureWidth, int textureHeight);
  ~Platform() {
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
  }

  void Update(void const* buffer, int pitch);
  bool ProcessInput(GameBoy& gameBoy);
  bool saveConfig();

 private:
  SDL_Window* window{};
  SDL_Renderer* renderer{};
  SDL_Texture* texture{};
  std::map<std::string, int> gbButtons;
  int keys[SDL_NUM_SCANCODES];
  void processKeysFromFile(std::ifstream& file);
};

#endif
