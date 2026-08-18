#include "GameBoy.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <system_error>

#include "../utils/Debug.hpp"
// The consturctor will initialize and then set the required state of the
// emulator as if the real game has started gameboy doesn't have an isolate
// stack that's why it is allocated at the near end of higher byte after the
// interept slot 0xFFFF
GameBoy::GameBoy()
    : m_programCounter(0x100),
      m_RegisterAF{.reg = 0x01B0},
      m_RegisterBC{.reg = 0x0013},
      m_RegisterDE{.reg = 0x00D8},
      m_RegisterHL{.reg = 0x014D},
      current_romBank(1),
      current_ramBank(0),
      current_vramBank(0),
      current_wramBank(1),

      m_MBC1(false),
      m_MBC2(false),
      m_MBC3(false),
      m_MBC5(false),
      m_mbc3RamBankOrRtc(0),
      m_mbc3RtcRegister(false),
      m_mbc3RtcIdx(0),
      m_RTCWriteState(0xFF),
      m_RTCaccumulator(0.0),
      m_enableRAM(false),
      m_enableROM(true),
      m_DividerCounter(0),
      m_joyPadState(0xFF),
      m_TimerCounter(0),
      m_scalineCounter(456),
      m_MasterInterrupt(false),
      m_EIpending(false),
      m_Halt(false),
      m_doubleSpeed(false),
      key_1(0),
      m_BGPaletteIndex(0),
      m_OBJPaletteIndex(0),
      m_autoIncBGPalette(false),
      m_autoIncOBJPalette(false),
      m_hdmaRemaining(0),
      m_hdmaActive(false),
      m_hdmaHBlankMode(false),
      m_hdmaLineDone(false) {
  m_stackPointer.reg = 0xFFFE;
  m_CartridgeMemory.assign(0x200000, 0);
  memset(&m_RTCLatch, 0, sizeof(m_RTCLatch));
  memset(&m_RTCregs, 0, sizeof(m_RTCregs));
  m_rom.assign(0x10000, 0);
  // memset(&m_ramBanks, 0, m_amBanks.size());
  // memset(&m_rom, 0, sizeof(m_rom));
  memset(&m_screenData, 0, sizeof(m_screenData));
  memset(&m_SerialOutput, 0, sizeof(m_SerialOutput));
  memset(&m_vram, 0, sizeof(m_vram));
  memset(&m_wram, 0, sizeof(m_wram));
  memset(&m_bgIndex, 0, sizeof(m_bgIndex));
  // CGB boot ROM initializes all background colors to white (RGB555
  // 0x7FFF). Without this, the title screen of games that rely on the
  // default palette (e.g. Pokemon Crystal) renders all-black.
  for (size_t i = 0; i < sizeof(m_BGPalette); i += 2) {
    m_BGPalette[i] = 0xFF;
    m_BGPalette[i + 1] = 0x7F;
    m_OBJPalette[i] = 0xFF;
    m_OBJPalette[i + 1] = 0x7F;
  }
  m_OBJPalette[0] = 0x00;  // boot ROM only sets OBJ0 color #0 low byte
  m_SerialIndex = 0;
  m_RestartDetected = false;

  m_rom[0xFF05] = 0x00;  // TIMA
  m_rom[0xFF06] = 0x00;  // TMA
  m_rom[0xFF07] = 0x00;  // TAC m_rom[0xFF10] = 0x80;  // NR10
  // m_rom[0xFF11] = 0xBF;  // NR11
  // m_rom[0xFF12] = 0xF3;  // NR12
  // m_rom[0xFF13] = 0;     // NR13
  // m_rom[0xFF14] = 0xBF;  // NR14
  // m_rom[0xFF16] = 0x3F;  // NR21
  // m_rom[0xFF17] = 0x00;  // NR22
  // m_rom[0xFF18] = 0x00;  // NR23
  // m_rom[0xFF19] = 0xBF;  // NE24
  // m_rom[0xFF1A] = 0x7F;  // NR30
  // m_rom[0xFF1B] = 0xFF;  // NR31
  // m_rom[0xFF1C] = 0x9F;  // NR32
  // m_rom[0xFF1D] = 0xBF;  // NR33
  // m_rom[0xFF1E] = 0x00;  // NR34
  // m_rom[0xFF20] = 0xFF;  // NR41
  // m_rom[0xFF21] = 0x00;  // NR42
  // m_rom[0xFF22] = 0x00;  // NR43
  // m_rom[0xFF23] = 0x00;  // NR44
  // m_rom[0xFF24] = 0x77;  // NR50
  // m_rom[0xFF25] = 0xF3;  // NR51
  // m_rom[0xFF26] = 0xF1;  // NR52
  m_rom[0xFF40] = 0x91;  // LCDC
  m_rom[0xFF42] = 0x00;  // SCY
  m_rom[0xFF43] = 0x00;  // SCX
  m_rom[0xFF45] = 0x00;  // LYC
  m_rom[0xFF47] = 0xE4;  // BGP
  m_rom[0xFF48] = 0xFF;  // OBP0
  m_rom[0xFF49] = 0xFF;  // OBP1
  m_rom[0xFF4A] = 0x00;  // WY
  m_rom[0xFF4B] = 0x00;  // WX
  m_rom[0xFFFF] = 0x00;  // IE
}

GameBoy::~GameBoy() {
}

int GameBoy::Update() {
  int cycles = NextOpCodeExcute();
  cycles += DoInterrupts();
  UpdateTimers(cycles);
  int newCycles = m_doubleSpeed ? cycles / 2 : cycles;
  // printf("[LOG] Program Counter before graphics:%d\n", m_programCounter);
  UpdateGraphics(newCycles);
  // printf("[LOG] Program Counter after graphics:%d\n", m_programCounter);
  apu.step(newCycles);
  return newCycles;
}

const byte* GameBoy::GetScreenData() const {
  return (const byte*)m_screenData;
}

// void GameBoy::RunTestMode(int maxFrames) {
//   Debug::Print("Running in headless test mode...\n");
//   Debug::PrintROMHeader(*this);
//
//   bool restarted = false;
//   static bool firstStart = true;
//   for (int frame = 0; frame < maxFrames && !restarted; frame++) {
//     int cyclesThisFrame = 0;
//     while (cyclesThisFrame < 70224) {
//       cyclesThisFrame += Update();
//
//       // Detect restart: if PC reaches the ROM entry point after first run
//       if (m_programCounter == 0x0637 && !m_RestartDetected) {
//         if (!firstStart) {
//           m_RestartDetected = true;
//         }
//         firstStart = false;
//       }
//
//       if (m_RestartDetected) {
//         Debug::Print("Stopping due to restart detection at frame %d.\n",
//         frame); restarted = true; break;
//       }
//     }
//     if (!restarted && (frame < 60 || frame % 1000 == 0)) {
//       Debug::PrintFrameInfo(frame, m_programCounter, m_stackPointer.reg,
//                             ReadMemory(0xFF0F), ReadMemory(0xFFFF));
//     }
//   }
//   // Printing out to reason
//   fprintf(stderr, "PC=%04x ,LY=%02x, LCDC=%02x, SCY =%02x,WY=%02x,WX=%02x\n",
//           ReadMemory(m_programCounter), ReadMemory(0xFF44),
//           ReadMemory(0xFF40), ReadMemory(0xFF42), ReadMemory(0xFF4A),
//           ReadMemory(0xFF4B));
//
//   for (int i = 0; i < 0x40; i += 2) {
//     fprintf(stderr, "%02x%02x", m_BGPalette[i + 1], m_BGPalette[i]);
//     fprintf(stderr, "\nhdmaActive=%d hdmaRemaining=%d hdmaHBlank=%d\n",
//             m_hdmaActive, m_hdmaRemaining, m_hdmaHBlankMode);
//   };
//   Debug::Print("Done. Screen output:\n\n");
//   Debug::DumpScreenASCII((const byte*)m_screenData);
//   {
//     FILE* f = fopen("/tmp/crystal_shot.ppm", "wb");
//     fprintf(f, "P6\n160 144\n255\n");
//     fwrite(m_screenData, 1, 160 * 144 * 4, f);
//     fclose(f);
//   }
//   Debug::PrintSerialOutput((const char*)m_SerialOutput);
// }

bool GameBoy::SaveRam(const char* savPath) {
  FILE* file = fopen(savPath, "wb");
  if (!file) {
    int errorCode = errno;
    std::string OS_errorMessage = std::generic_category().message(errorCode);
    throw std::runtime_error(
        "Failed to create save file. Reason: " + OS_errorMessage +
        " (Code: " + std::to_string(errorCode) + ")");
  }
  std::unique_ptr<saveData> data = convertFormat();
  size_t ramBanksSize = data->ramBanks.size();
  fwrite(&ramBanksSize, 1, sizeof(size_t), file);
  fwrite(data->ramBanks.data(), 1, ramBanksSize, file);
  fwrite(data->RTCregs, sizeof(data->RTCregs), 1, file);
  fwrite(&data->RTCTimeStamp, sizeof(TimePoint), 1, file);
  fclose(file);
  return true;
}

std::unique_ptr<saveData> GameBoy::convertFormat() const {
  auto data = std::make_unique<saveData>();
  memcpy(data->RTCregs, m_RTCregs, sizeof(m_RTCregs));
  data->ramBanks.resize(m_ramBanks.size());
  data->ramBanks = m_ramBanks;
  data->RTCTimeStamp = Clock::now();
  return data;
};

void GameBoy::LoadRam(const char* loadPath) {
  FILE* file = fopen(loadPath, "rb");
  if (!file) {
    return;
  }
  size_t ramBanksSize = 0;
  fread(&ramBanksSize, sizeof(size_t), 1, file);
  if (ramBanksSize != m_ramBanks.size()) {
    fprintf(stderr, "The ram size doens't match resize back to default");
    fread(m_RTCregs, sizeof(m_RTCregs), 1, file);
    TimePoint timestamp;
    fread(&timestamp, sizeof(TimePoint), 1, file);
    m_RTCtimeStamp = timestamp;
    std::chrono::duration<float> elasped_time = (Clock::now() - m_RTCtimeStamp);
    fastForwardRTC(elasped_time.count());
  } else {
    std::vector<byte> loadedRam(ramBanksSize);
    fread(loadedRam.data(), 1, ramBanksSize, file);
    m_ramBanks = std::move(loadedRam);
  }
  fclose(file);
}

word* GameBoy::getBG_Palette() const {
  word* data = new word[32];
  for (int i = 0; i < 0x40; i += 2) {
    data[i / 2] = (m_BGPalette[i + 1] << 8) | (m_BGPalette[i]);
  }
  return data;
}

const byte* GameBoy::GetVram(int bank) {
  return m_vram[bank];
}

// word* GameBoy::getOBJ_Palette() const {
//   word* data = new word[32];
// }
