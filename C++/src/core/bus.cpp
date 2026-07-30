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
    m_MBC1 = true;
  } else if (mbcType == 5 || mbcType == 6) {
    m_MBC2 = true;
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
    m_Div = 0;
    m_DividerCounter = 0;
    return;
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
      if (m_MBC3 && m_mbc3RtcRegister) {
        WriteRTCReg(data);
      } else {
        word new_addr = address - 0xA000;
        m_ramBanks[new_addr + (current_ramBank * 0x2000)] = data;
      }
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
    if (!m_enableRAM)
      return 0xFF;
    if (m_MBC3 && m_mbc3RtcRegister) {
      return ReadRTCRegister();
    }
    // shif the offset
    word new_addr = address - 0xA000;
    // each ram bank is 0x2000
    return m_ramBanks[new_addr + (current_ramBank * 0x2000)];
  }

  // joypad register - return computed button state
  else if (address == 0xFF00) {
    return GetJoyPadState();
  }

  else if (address == 0xFF04) {
    return m_Div;
  }

  // others region? return
  else {
    return m_rom[address];
  }
}
void GameBoy::WriteRTCReg(byte data) {
  byte idx = m_mbc3RtcIdx - 0x08;
  if (idx == 4) {
    m_RTCregs[idx] = data & 0xC1;
    return;
  }
  m_RTCregs[idx] = data;
};

void GameBoy::HandleBanking(word address, byte data) {
  // do RAM enabling
  if (address < 0x2000) {
    if (m_MBC1 || m_MBC2 || m_MBC3) {
      DoRAMBanking(address, data);
    }
  }
  // do RAM switching
  else if (address >= 0x2000 && address <= 0x3FFF) {
    if (m_MBC1 || m_MBC2 || m_MBC3) {
      DoChangeLoROMBank(data);
    }
  }
  // do RAM or ROM switching
  else if (address >= 0x4000 && address < 0x6000) {
    // no ramBank in Mbu2 alwasy uses ramBank 0
    if (m_MBC1) {
      if (m_enableROM) {
        DoChangeHiROMBank(data);
      } else {
        DoChangeRAMBank(data);
      }
    } else if (m_MBC3) {
      doChangeRamOrRTC(data);
    }
  }
  // do ROM enable
  else if (address >= 0x6000 && address < 0x8000) {
    if (m_MBC1) {
      DoChangeROMRAMBank(data);
    } else if (m_MBC3) {
      handleRTCLatch(data);
    }
  }
}
void GameBoy::handleRTCLatch(byte data) {
  if ((m_RTCWriteState == 0x00) && (data == 0x01)) {
    std::memcpy(m_RTCLatch, m_RTCregs, sizeof(m_RTCregs));
  }
  m_RTCWriteState = data;
}

void GameBoy::doChangeRamOrRTC(byte data) {
  if (data <= 3) {
    current_ramBank = data;
    m_mbc3RtcRegister = false;
  } else if (data >= 0x08 && data <= 0x0C) {
    m_mbc3RtcRegister = true;
    m_mbc3RtcIdx = data;
  }
}

void GameBoy::DoRAMBanking(word address, byte data) {
  if (m_MBC2) {
    if ((address & 0x10) != 0) {
      return;
    }
  }

  byte new_data = data & 0xF;
  m_enableRAM = new_data == 0xA;
}

void GameBoy::DoChangeLoROMBank(byte data) {
  if (m_MBC2) {
    current_romBank = data & 0xF;
    if (current_romBank == 0) {
      current_romBank++;
    }
    return;
  }
  if (m_MBC3) {
    current_romBank = data & 0x7F;
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
  current_romBank &= 31;               // take out 5 bit
  byte new_data = (data & 0x03) << 5;  // take out 3 bit
  current_romBank |= new_data;         // merge
  if (current_romBank == 0) {
    current_romBank++;
  }
}

// Only take out the lower 2 bit
// Max Bank count support in MBC1 = 4
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

byte GameBoy::ReadRTCRegister() const {
  byte idx = m_mbc3RtcIdx - 0x08;
  return m_RTCregs[idx];
};
