#include <cstdio>
#include <cstring>

#include "../utils/Debug.hpp"
#include "GameBoy.hpp"

void GameBoy::ReadRom(char const* filePath) {
  memset(&m_CartridgeMemory, 0, sizeof(m_CartridgeMemory));
  FILE* in;

  in = fopen(filePath, "rb");
  if (!in) {
    Debug::Error("Could not open ROM file: %s\n", filePath);
    return;
  }
  fread(m_CartridgeMemory, 1, 0x200000, in);
  fclose(in);

  byte mbcType = m_CartridgeMemory[0x147];
  if (mbcType >= 1 && mbcType <= 3) {
    m_MBU1 = true;
  } else if (mbcType == 5 || mbcType == 6) {
    m_MBU2 = true;
  } else if (mbcType >= 0x0F && mbcType <= 0x13) {
    m_MBC3 = true;
  }
}

void GameBoy::WriteMemory(word address, byte data) {
  // if within the switching range then handle the switching
  if (address < 0x8000) {
    HandleBanking(address, data);
  }
  // if writing to the dividerRegister
  else if (address == 0xFF04) {
    m_rom[0xFF04] = 0;  // reset to 0
  }
  // serial output (Blargg test output)
  else if (address == 0xFF01) {
    m_rom[0xFF01] = data;
  } else if (address == 0xFF02) {
    m_rom[0xFF02] = data;
    if (data == 0x81) {  // serial transfer start
      byte ch = m_rom[0xFF01];
      if (ch != 0 && m_SerialIndex < 255) {
        m_SerialOutput[m_SerialIndex++] = ch;
        m_SerialOutput[m_SerialIndex] = '\0';
      }
    }
  }

  // if writing to the scanline
  else if (address == 0xFF44) {
    m_rom[0xFF44] = 0;  // reset
  }
  // if writing to the dma source address
  else if (address == 0xFF46) {
    DoDMATransfer(data);
  }

  // if writing to TMC
  else if (address == 0xFF07) {
    byte currentFeq = GetClockFeq();
    m_rom[0xFF07] = data;  // this can change the enable bit or feq bits
    byte newFeq = GetClockFeq();
    if (currentFeq != newFeq) {  //  change in feq
      SetClockFeq();             // set new one
    }
  }

  // if within the switchable ram range
  else if (address >= 0xA000 && address < 0xC000) {
    if (m_enableRAM) {
      word new_addr = address - 0xA000;
      m_ramBanks[new_addr + (current_ramBank * 0x2000)] = data;
    }
  }

  // writing in echo region
  else if ((address >= 0xE000) && (address < 0xFE00)) {
    m_rom[address] = data;
    WriteMemory(address - 0x2000, data);  // has to write into wram space too
  }
  // no write allowed for these regions
  else if (address >= 0xFEA0 && address < 0xFEFF) {
  }
  // others
  else {
    m_rom[address] = data;
  }
}

byte GameBoy::ReadMemory(word address) const {
  // are we reading from rom bank 0 (fixed, always first 16KB of cartridge)
  if (address < 0x4000) {
    return m_CartridgeMemory[address];
  }

  // are we reading from switchable rom
  if (address >= 0x4000 && address <= 0x7FFF) {
    // shift the offset to start;
    word new_addr = address - 0x4000;
    // each rom bank is 0x4000 so have to multiply to offset it
    return m_CartridgeMemory[new_addr + (current_romBank * 0x4000)];
  }

  // are we reading from switchable ram
  else if (address >= 0xA000 && address <= 0xBFFF) {
    // shif the offset
    word new_addr = address - 0xA000;
    // each ram bank is 0x2000
    return m_ramBanks[new_addr + (current_ramBank * 0x2000)];
  }

  // joypad register - return computed button state
  else if (address == 0xFF00) {
    return GetJoyPadState();
  }

  // others region? return
  else {
    return m_rom[address];
  }
}

void GameBoy::HandleBanking(word address, byte data) {
  // MBC3 handling
  if (m_MBC3) {
    if (address < 0x2000) {
      // RAM/Timer Enable
      m_enableRAM = ((data & 0x0F) == 0x0A);
    } else if (address >= 0x2000 && address <= 0x3FFF) {
      // ROM Bank Number (0x00 maps to 0x01)
      byte bank = data & 0x7F;
      if (bank == 0)
        bank = 1;
      current_romBank = bank;
    } else if (address >= 0x4000 && address <= 0x5FFF) {
      // RAM Bank Number or RTC Register Select
      m_mbc3RamBankOrRtc = data;
      if (data <= 3) {
        current_ramBank = data;
        m_mbc3RtcRegister = false;
      } else if (data >= 0x08 && data <= 0x0C) {
        m_mbc3RtcRegister = true;
      }
    }
    return;
  }

  // do RAM enabling
  if (address < 0x2000) {
    if (m_MBU1 || m_MBU2) {
      DoRAMBanking(address, data);
    }
  }
  // do RAM switching
  else if (address >= 0x2000 && address <= 0x3FFF) {
    if (m_MBU1 || m_MBU2) {
      DoChangeLoROMBank(data);
    }
  }
  // do RAM or ROM switching
  else if (address >= 0x4000 && address < 0x6000) {
    // no ramBank in Mbu2 alwasy uses ramBank 0
    if (m_MBU1) {
      if (m_enableROM) {
        DoChangeHiROMBank(data);
      } else {
        DoChangeRAMBank(data);
      }
    }
  }
  // do ROM enable
  else if (address >= 0x6000 && address < 0x8000) {
    if (m_MBU1) {
      DoChangeROMRAMBank(data);
    }
  }
}

void GameBoy::DoRAMBanking(word address, byte data) {
  if (m_MBU2) {
    if ((address & 0x10) == 1) {
      return;
    }
  }
  byte new_data = data & 0xF;
  if (new_data == 0xA) {
    m_enableRAM = true;
  } else if (new_data == 0x0) {
    m_enableRAM = false;
  }
}

void GameBoy::DoChangeLoROMBank(byte data) {
  if (m_MBU2) {
    current_romBank = data & 0xF;
    if (current_romBank == 0) {
      current_romBank++;
    }
    return;
  }
  // 31 in binary =0001 1111
  byte low5 = data & 31;  // lower 5 bits
  // 224 in binary = 1110 0000
  current_romBank &= 224;   // turn of lower 5 bit
  current_romBank |= low5;  // merge
  if (current_romBank == 0) {
    current_romBank++;
  }
}

void GameBoy::DoChangeHiROMBank(byte data) {
  current_romBank &= 31;        // take out 5 bit
  byte new_data = data & 224;   // take out 3 bit
  current_romBank |= new_data;  // merge
  if (current_romBank == 0) {
    current_romBank++;
  }
}

// Only take out the lower 2 bit
// Max Bank count support in MBU1 = 4
// 00 =0
// 01 =1
// 10 =2
// 11 =3
void GameBoy::DoChangeRAMBank(byte data) {
  current_ramBank = data & 3;
}

void GameBoy::DoChangeROMRAMBank(byte data) {
  byte new_data = data & 0x1;  // take out 1 bit
  m_enableROM = (new_data == 0) ? true : false;
  // reset  ramBank to 0
  if (m_enableROM) {
    current_ramBank = 0;
  }
}
