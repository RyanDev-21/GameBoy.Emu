#include "GameBoy.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <stdexcept>

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
      m_MBU1(false),
      m_MBU2(false),
      m_MBC3(false),
      m_mbc3RamBankOrRtc(0),
      m_mbc3RtcRegister(false),
      m_enableRAM(false),
      m_enableROM(true),
      m_DividerCounter(0),
      m_joyPadState(0xFF),
      m_TimerCounter(0),
      m_scalineCounter(456),
      m_MasterInterrupt(false),
      m_EIpending(false),
      m_Halt(false) {
  m_stackPointer.reg = 0xFFFE;
  memset(&m_ramBanks, 0, sizeof(m_ramBanks));
  memset(&m_rom, 0, sizeof(m_rom));
  memset(&m_screenData, 0, sizeof(m_screenData));
  memset(&m_SerialOutput, 0, sizeof(m_SerialOutput));
  m_SerialIndex = 0;
  m_RestartDetected = false;

  m_rom[0xFF05] = 0x00;  // TIMA
  m_rom[0xFF06] = 0x00;  // TMA
  m_rom[0xFF07] = 0x00;  // TAC
  m_rom[0xFF10] = 0x80;  // NR10
  m_rom[0xFF11] = 0xBF;  // NR11
  m_rom[0xFF12] = 0xF3;  // NR12
  m_rom[0xFF14] = 0xBF;  // NR14
  m_rom[0xFF16] = 0x3F;  // NR21
  m_rom[0xFF17] = 0x00;  // NR22
  m_rom[0xFF19] = 0xBF;  // NE24
  m_rom[0xFF1A] = 0x7F;  // NR30
  m_rom[0xFF1B] = 0xFF;  // NR31
  m_rom[0xFF1C] = 0x9F;  // NR32
  m_rom[0xFF1E] = 0xBF;  // NR33
  m_rom[0xFF20] = 0xFF;  // NR41
  m_rom[0xFF21] = 0x00;  // NR42
  m_rom[0xFF22] = 0x00;  // NR43
  m_rom[0xFF23] = 0xBF;  // NR30
  m_rom[0xFF24] = 0x77;  // NR50
  m_rom[0xFF25] = 0xF3;  // NR51
  m_rom[0xFF26] = 0xF1;  // NR52
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

int GameBoy::Update() {
  int cycles = NextOpCodeExcute();
  UpdateTimers(cycles);
  UpdateGraphics(cycles);
  DoInterrupts();
  return cycles;
}

const byte* GameBoy::GetScreenData() const {
  return (const byte*)m_screenData;
}

void GameBoy::RunTestMode(int maxFrames) {
  Debug::Print("Running in headless test mode...\n");
  Debug::PrintROMHeader(*this);

  bool restarted = false;
  static bool firstStart = true;
  for (int frame = 0; frame < maxFrames && !restarted; frame++) {
    int cyclesThisFrame = 0;
    while (cyclesThisFrame < 70224) {
      cyclesThisFrame += Update();

      // Detect restart: if PC reaches the ROM entry point after first run
      if (m_programCounter == 0x0637 && !m_RestartDetected) {
        if (!firstStart) {
          m_RestartDetected = true;
        }
        firstStart = false;
      }

      if (m_RestartDetected) {
        Debug::Print("Stopping due to restart detection at frame %d.\n", frame);
        restarted = true;
        break;
      }
    }
    if (!restarted && (frame < 60 || frame % 1000 == 0)) {
      Debug::PrintFrameInfo(frame, m_programCounter, m_stackPointer.reg,
                            ReadMemory(0xFF0F), ReadMemory(0xFFFF));
    }
  }
  Debug::Print("Done. Screen output:\n\n");
  Debug::DumpScreenASCII((const byte*)m_screenData);
  Debug::PrintSerialOutput((const char*)m_SerialOutput);
}

bool GameBoy::SaveRam(const char* savPath) {
  FILE* file = fopen(savPath, "wb");
  if (!file) {
    int errorCode = errno;
    std::string OS_errorMessage = std::generic_category().message(errorCode);

    // Include the specific OS error inside your throw message
    throw std::runtime_error(
        "Failed to create save file. Reason: " + OS_errorMessage +
        " (Code: " + std::to_string(errorCode) + ")");
  }
  fwrite(m_ramBanks, 1, sizeof(m_ramBanks), file);
  fclose(file);
  return true;
}

void GameBoy::LoadRam(const char* loadPath) {
  FILE* file = fopen(loadPath, "rb");
  if (!file) {
    return;
  }
  fread(m_ramBanks, 1, sizeof(m_ramBanks), file);
  fclose(file);
}
