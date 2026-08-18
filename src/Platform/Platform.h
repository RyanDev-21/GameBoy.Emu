#ifndef PLATFORM_H
#define PLATFORM_H
#include <SDL2/SDL.h>
#include <SDL2/SDL_scancode.h>

#include <fstream>
#include <iostream>
#include <map>

#include "../../imgui/imgui_impl_sdl2.h"
#include "../../imgui/imgui_impl_sdlrenderer2.h"
#include "../core/GameBoy.hpp"
class Platform {
 public:
  Platform(char const* title, int windowWidth, int windowHeight,
           int textureWidth, int textureHeight);
  ~Platform() {
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
  }

  void Update(void const* buffer, int pitch);

  void UpdateWithDebug(void const* buffer, int pitch, const GameBoy* gameboy);
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
