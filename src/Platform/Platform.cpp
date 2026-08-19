#include "Platform.h"

#include <SDL2/SDL_events.h>
#include <SDL2/SDL_keyboard.h>
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_scancode.h>

#include <cstdio>
#include <fstream>

#include "../../imgui/imgui.h"
#include "../../imgui/imgui_impl_sdl2.h"
#include "../../imgui/imgui_impl_sdlrenderer2.h"
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
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |=
      ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
  io.ConfigFlags |=
      ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls
  ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
  ImGui_ImplSDLRenderer2_Init(renderer);
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
  // build reverse map: button -> scancode
  for (int i = 0; i < 8; ++i) buttonScancodes[i] = -1;
  for (int sc = 0; sc < SDL_NUM_SCANCODES; ++sc) {
    int b = keys[sc];
    if (b >= 0 && b < 8) buttonScancodes[b] = sc;
  }
}

void Platform::Update(void const* buffer, int pitch) {
  SDL_UpdateTexture(texture, nullptr, buffer, pitch);
  SDL_RenderCopy(renderer, texture, nullptr, nullptr);
  SDL_RenderPresent(renderer);
}
void Platform::UpdateWithDebug(void const* buffer, int pitch,
                               const GameBoy* gameboy) {
  ImGui_ImplSDLRenderer2_NewFrame();
  ImGui_ImplSDL2_NewFrame();
  ImGui::NewFrame();
  timer timer = gameboy->getTimerState();
  Internal internal = gameboy->getCPUState();
  Interrupt interrupt = gameboy->getInterruptState();
  MBC mbc = gameboy->getMBCState();
  HDMA hdma = gameboy->getHDMAState();
  ImGui::Begin("GameBoy Debugger");
  ImGui::Text("FPS:%.1f", ImGui::GetIO().Framerate);
  ImGui::Text("Program Counter:%#06x", gameboy->m_programCounter);
  if (ImGui::TreeNode("PPU")) {
    ImGui::Text("Scanline Counter:%d", gameboy->ReadMemory(0xFF44));
    ImGui::Text("WindowY:%d", gameboy->ReadMemory(0xFF4A));
    ImGui::Text("WindowX:%d", gameboy->ReadMemory(0xFF4B));
    ImGui::Text("Lcdc:%d", gameboy->ReadMemory(0xFF40));
    ImGui::TreePop();
  }
  if (ImGui::TreeNode("Timer")) {
    ImGui::Text("TIMA :%d", timer.tima);
    ImGui::Text("TMA:%d", timer.tma);
    ImGui::Text("TMC:%d", timer.tmc);
    if (ImGui::TreeNode("RTC Register")) {
      if (ImGui::TreeNode("register value")) {
        for (int i = 0; i < 5; i++) {
          ImGui::Text("Reg[%d]: 0x%02X", i, timer.rtc.regs[i]);
        }
        ImGui::TreePop();
      }
      if (ImGui::TreeNode("latch value")) {
        for (int i = 0; i < 5; i++) {
          ImGui::Text("Latch[%d]: 0x%02X", i, timer.rtc.latch[i]);
        }
        ImGui::TreePop();
      }
      ImGui::Text("RTC accumulator:%f", timer.rtc.RTCaccumulator);
      ImGui::Text("RTC write state:%s",
                  timer.rtc.RTCWriteState ? "True" : "False");
      ImGui::Text("MBC3 RTC reg:%s", timer.rtc.mbc3RTCreg ? "True" : "False");
      ImGui::TreePop();
    }
    ImGui::TreePop();
  }
  if (ImGui::TreeNode("CPU")) {
    ImGui::Text("isGBC : %s", internal.isGBC ? "True" : "False");
    ImGui::Text("RegisterAF: %d", internal.RegisterAF);
    ImGui::Text("RegisterBC: %d", internal.RegisterBC);
    ImGui::Text("RegisterDE: %d", internal.RegisterDE);
    ImGui::Text("RegisterHL: %d", internal.RegisterHL);
    ImGui::Text("StackPointer: %d", internal.StackPointer);
    ImGui::Text("RamSize: %zu", internal.ramSize);
    ImGui::Text("Double Speed: %s", internal.doubleSpeed ? "True" : "False");
    ImGui::TreePop();
  }
  if (ImGui::TreeNode("Interrupt")) {
    ImGui::Text("Master Interrupt: %s",
                interrupt.masterInterrupt ? "True" : "False");
    ImGui::Text("EIpending: %s", interrupt.EIpending ? "True" : "False");
    ImGui::Text("halt: %s", interrupt.halt ? "True" : "False");
    ImGui::Text("halt_Bug: %s", interrupt.halt_Bug ? "True" : "False");
    ImGui::Text("previousStatusLine: %s",
                interrupt.previousStatusLine ? "True" : "False");

    ImGui::TreePop();
  }
  if (ImGui::TreeNode("HDMA")) {
    ImGui::Text("hdma active: %s", hdma.hdmaActive ? "True" : "False");
    ImGui::Text("hdmaHBlankMode: %s", hdma.hdmaHBlankMode ? "True" : "False");
    ImGui::Text("hdmaLineDone: %s", hdma.hdmaLineDone ? "True" : "False");
    ImGui::Text("hdmaRemaining: %d", hdma.hdmaRemaining);
    ImGui::TreePop();
  }

  if (ImGui::TreeNode("MBC")) {
    ImGui::Text("Current ramBank:%d", mbc.current_ramBank);
    ImGui::Text("Current romBank:%d", mbc.current_romBank);
    ImGui::Text("Current vramBank:%d", mbc.current_vramBank);
    ImGui::Text("Current wramBank:%d", mbc.current_wramBank);
    ImGui::Text("Enabled ram: %s", mbc.enabled_ram ? "True" : "False");
    ImGui::Text("Enabled rom: %s", mbc.enabled_rom ? "True" : "False");
    ImGui::Text("MBC1:%s", mbc.MBC1 ? "True" : "False");
    ImGui::Text("MBC2:%s", mbc.MBC2 ? "True" : "False");
    ImGui::Text("MBC3:%s", mbc.MBC3 ? "True" : "False");
    ImGui::Text("MBC5:%s", mbc.MBC5 ? "True" : "False");

    ImGui::TreePop();
  }
  if (ImGui::TreeNode("CartridgeMemory")) {
    std::vector<byte> CartridgeMemory = gameboy->getCartridgeMemory();
    word romBank = mbc.current_romBank;
    word file_offset = (romBank * 0x4000) + (0x49cd - 0x4000);
    for (int i = -4; i < 12; i++) {
      ImGui::Text("Value[%d]: 0x%02X", i, CartridgeMemory[file_offset + i]);
    }
    ImGui::TreePop();
  }
  // Keymap editor UI
  if (ImGui::CollapsingHeader("Input / Keymap")) {
    if (ImGui::Button("Load from file")) {
      std::ifstream file("gameboy_gamePad.config");
      if (file.is_open()) {
        processKeysFromFile(file);
        // refresh reverse map
        for (int i = 0; i < 8; ++i) buttonScancodes[i] = -1;
        for (int sc = 0; sc < SDL_NUM_SCANCODES; ++sc) {
          int b = keys[sc];
          if (b >= 0 && b < 8) buttonScancodes[b] = sc;
        }
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Save to file")) {
      saveConfig();
    }
    ImGui::SameLine();
    if (ImGui::Button("Restore Defaults")) {
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
      for (int i = 0; i < SDL_NUM_SCANCODES; i++) keys[i] = -1;
      for (int i = 0; i < 8; i++) {
        keys[default_keys[i]] = i;
        buttonScancodes[i] = default_keys[i];
      }
    }

    ImGui::Spacing();
    ImGui::Text("Click 'Capture' then press a key to assign.");
    const char* button_names[8] = {"RIGHT", "LEFT", "UP", "DOWN",
                                   "A",     "B",    "SELECT", "START"};
    for (int i = 0; i < 8; ++i) {
      ImGui::PushID(i);
      ImGui::TextUnformatted(button_names[i]);
      ImGui::SameLine(150);
      const char* scname = "(unassigned)";
      if (buttonScancodes[i] >= 0) scname = SDL_GetScancodeName((SDL_Scancode)buttonScancodes[i]);
      ImGui::TextUnformatted(scname);
      ImGui::SameLine(350);
      if (ImGui::Button(capturing && captureTarget==i ? "..." : "Capture")) {
        capturing = true;
        captureTarget = i;
      }
      ImGui::PopID();
    }
  }

  ImGui::End();
  ImGui::Render();
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_RenderClear(renderer);
  SDL_UpdateTexture(texture, nullptr, buffer, pitch);
  SDL_RenderCopy(renderer, texture, nullptr, nullptr);
  ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
  SDL_RenderPresent(renderer);
}

bool Platform::ProcessInput(GameBoy& gameBoy) {
  bool quit = false;

  SDL_Event event;

  while (SDL_PollEvent(&event)) {
    ImGui_ImplSDL2_ProcessEvent(&event);
    if (event.type == SDL_QUIT) {
      quit = true;
    }
    if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
      // If capturing for remap, consume the next KEYDOWN and assign
      if (event.type == SDL_KEYDOWN && capturing && captureTarget >= 0) {
        SDL_Scancode sc = event.key.keysym.scancode;
        // If this scancode was assigned to another button, clear that button
        int prevBtn = keys[sc];
        if (prevBtn >= 0 && prevBtn < 8) {
          buttonScancodes[prevBtn] = -1;
        }
        // Clear old scancode for this target
        int oldSc = buttonScancodes[captureTarget];
        if (oldSc >= 0 && oldSc < SDL_NUM_SCANCODES) {
          keys[oldSc] = -1;
        }
        // Assign new mapping
        keys[sc] = captureTarget;
        buttonScancodes[captureTarget] = sc;
        capturing = false;
        captureTarget = -1;
        continue;
      }

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

void Platform::refreshButtonScancodes() {
  for (int i = 0; i < 8; ++i) buttonScancodes[i] = -1;
  for (int sc = 0; sc < SDL_NUM_SCANCODES; ++sc) {
    int b = keys[sc];
    if (b >= 0 && b < 8) buttonScancodes[b] = sc;
  }
}

bool Platform::saveConfig() {
  std::ofstream file("gameboy_gamePad.config", std::ios::trunc);
  if (!file.is_open()) return false;
  const char* button_names[8] = {"RIGHT", "LEFT", "UP", "DOWN",
                                 "A",     "B",    "SELECT", "START"};
  for (int i = 0; i < 8; ++i) {
    const char* scname = "";
    if (buttonScancodes[i] >= 0) scname = SDL_GetScancodeName((SDL_Scancode)buttonScancodes[i]);
    file << button_names[i] << " = " << scname << "\n";
  }
  file.close();
  return true;
}
