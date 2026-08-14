#include <algorithm>
#include <cstdio>
#include <cstring>

#include "../utils/Debug.hpp"
#include "GameBoy.hpp"

void GameBoy::ReadRom(char const* filePath) {
  m_CartridgeMemory.clear();
  FILE* in;
  in = fopen(filePath, "rb");
  if (!in) {
    Debug::Error("Could not open ROM file: %s\n", filePath);
    return;
  }
  fseek(in, 0, SEEK_END);

  long size = ftell(in) > 0x800000 ? 0x800000 : ftell(in);
  fseek(in, 0, SEEK_SET);

  m_CartridgeMemory.resize(size);
  fread(m_CartridgeMemory.data(), 1, size, in);
  fclose(in);
  ramSize = GetRamSize(m_CartridgeMemory[0x149]);
  m_ramBanks.assign(ramSize, 0);
  byte mbcType = m_CartridgeMemory[0x147];
  if (mbcType >= 1 && mbcType <= 3) {
    m_MBC1 = true;
  } else if (mbcType == 5 || mbcType == 6) {
    m_MBC2 = true;
  } else if (mbcType >= 0x0F && mbcType <= 0x13) {
    m_MBC3 = true;
  } else if (mbcType >= 0x19 && mbcType <= 0x1E) {
    m_MBC5 = true;
  }
  // bit 7 set for GCB compatible and DMG also
  // bit 7 and 6 set for only CGB
  m_isGBC =
      (m_CartridgeMemory[0x143] == 0x80 || m_CartridgeMemory[0x143] == 0xC0);

  // The boot ROM leaves register A = 0x11 when booting in CGB mode (0x01 on
  // DMG).
  m_RegisterAF.reg = m_isGBC ? 0x11B0 : 0x01B0;
}
size_t GameBoy::GetRomSize(byte code) {
  if (code <= 0x08) {
    return size_t(32 * (1 << (code & 0x01)));
  }
  return 32 * 1024;
}
size_t GameBoy::GetRamSize(byte code) {
  switch (code) {
    case 0x00: return 0; break;
    case 0x01: return 0; break;
    case 0x02: return 8 * 1024; break;
    case 0x03: return 32 * 1024; break;
    case 0x04: return 128 * 1024; break;
    case 0x05: return 54 * 1024; break;
    default: return 0;
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
  } else if (address >= 0xFF10 && address <= 0xFF3F) {
    apu.handleWriteRouting(address, data);
  }
  // for SGB
  else if (address == 0xFF4F) {
    WriteVBK(data);
    m_rom[0xFF4F] = data;
  } else if (address == 0xFF70) {
    WriteSVBK(data);
    m_rom[0xFF70] = data;
  } else if (address == 0xFF68) {
    WriteBCPS(data);
  } else if (address == 0xFF69) {
    WriteBCPD(data);
  } else if (address == 0xFF6A) {
    WriteOCPS(data);
  } else if (address == 0xFF6B) {
    WriteOCPD(data);
  } else if (address == 0xFF4D) {
    key_1 = (key_1 & 0x80) | (data & 0x01);
    m_rom[0xFF4D] = key_1;
  }
  // HDMA source/dest address registers (high bytes); low nibbles masked
  // thes tow are source addr
  else if (address == 0xFF51) {
    m_rom[0xFF51] = data;
  } else if (address == 0xFF52) {
    m_rom[0xFF52] = data;
  }
  // this two reg is destination addr
  else if (address == 0xFF53) {
    m_rom[0xFF53] = data;
  } else if (address == 0xFF54) {
    m_rom[0xFF54] = data;
  }
  // transfer length/mode/start reg
  // the reason we need to dec the data&0x7F is because
  // when the gameboy write the lenght of that data they want to copy
  // they alwasy write as length-1 so in order to get the actual length
  // we need to inc that back again
  else if (address == 0xFF55) {
    // checking that if we have currentlly running hdmaTransfer
    // while the gdma bit 7 is turned on?
    if (!(data & 0x80) && m_hdmaActive && m_hdmaHBlankMode) {
      m_hdmaActive = false;
      m_rom[0xFF55] = 0x80 | ((byte)(m_hdmaRemaining - 1) & 0x7F);
      return;
    }
    m_rom[0xFF55] = data;
    if (data & 0x80) {
      // HBlank DMA: one 16-byte chunk is copied during each HBlank
      m_hdmaActive = true;
      m_hdmaHBlankMode = true;
      m_hdmaLineDone = false;
      m_hdmaRemaining = (word)(data & 0x7F) + 1;
    } else {
      // General purpose DMA: copy the whole block immediately
      m_hdmaActive = true;
      m_hdmaHBlankMode = false;
      m_hdmaRemaining = (word)(data & 0x7F) + 1;
      while (m_hdmaRemaining > 0) {
        DoHDMAChunk();
      }
      m_hdmaActive = false;
      m_rom[0xFF55] = 0xFF;  // 0xFF = transfer finished
    }
  }
  // for writing data for GBC
  else if (address >= 0x8000 && address <= 0x9FFF) {
    m_vram[current_vramBank][address - 0x8000] = data;
  }

  else if (address >= 0xC000 && address <= 0xCFFF) {
    m_wram[0][address - 0xC000] = data;
  } else if (address >= 0xD000 && address <= 0xDFFF) {
    m_wram[current_wramBank][address - 0xD000] = data;

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
  // for GBC
  else if (address >= 0x8000 && address <= 0x9FFF) {
    return m_vram[current_vramBank][address - 0x8000];
  }

  else if (address >= 0xC000 && address <= 0xCFFF) {
    return m_wram[0][address - 0xC000];  // this is the fixed acccess
  }

  else if (address >= 0xD000 && address <= 0xDFFF) {
    return m_wram[current_wramBank][address - 0xD000];
  } else if (address >= 0xFF10 && address <= 0xFF3F) {
    // fprintf(stderr, "APU read addr=%04X\n", address);
    apu.handleReadRouting(address);
  } else if (address == 0xFF4D) {
    return m_rom[0xFF4D];
  }
  // others region? return
  return m_rom[address];
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
    if (m_MBC1 || m_MBC2 || m_MBC3 || m_MBC5) {
      DoRAMBanking(address, data);
    }
  }
  // do RAM switching
  else if (address >= 0x2000 && address <= 0x3FFF) {
    if (m_MBC5) {
      if (address <= 0x2FFF) {
        current_romBank = data & 0xFF;
      } else if (address >= 0x3000 && address <= 0x3FFF) {
        current_romBank = ((data & 0x01) << 8) | current_romBank;
      }
    }
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
    if (m_MBC5) {
      current_ramBank = data & 0xFF;
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

void GameBoy::WriteVBK(byte data) {
  current_vramBank = data & 0x01;
}

void GameBoy::WriteSVBK(byte data) {
  byte bank = data & 0x07;
  // if zero has to set to 1
  current_wramBank = (bank == 0) ? 1 : bank;
}

void GameBoy::WriteBCPS(byte data) {
  // Bits:0-5 (0-63)
  m_BGPaletteIndex = data & 0x3F;
  // auto inc or not is check by the 7 bit last bit
  m_autoIncBGPalette = (data & 0x80) != 0;
}

void GameBoy::WriteBCPD(byte data) {
  m_BGPalette[m_BGPaletteIndex] = data;
  // m_BGPalette max size is 0x40 so we have to mask it
  if (m_autoIncBGPalette) {
    m_BGPaletteIndex = (m_BGPaletteIndex + 1) & 0x3F;
  }
}

void GameBoy::WriteOCPS(byte data) {
  m_OBJPaletteIndex = data & 0x3F;
  // auto inc or not is check by the 7 bit last bit
  m_autoIncOBJPalette = (data & 0x80) != 0;
}

void GameBoy::WriteOCPD(byte data) {
  m_OBJPalette[m_OBJPaletteIndex] = data;
  // m_OBJPalette max size is 0x40 so we have to mask it
  if (m_autoIncOBJPalette) {
    m_OBJPaletteIndex = (m_OBJPaletteIndex + 1) & 0x3F;
  }
}

// Copies a single 16-byte chunk from the HDMA source to the VRAM destination,
// then advances the source/dest pointers by 16.
// Source is an absolute address:      (FF51<<8) | (FF52 & 0xF0)
// Destination is an offset from VRAM: 0x8000 | ((FF53 & 0x1F) << 8) | (FF54 &
// 0xF0)
void GameBoy::DoHDMAChunk() {
  word src = ((word)m_rom[0xFF51] << 8) | (m_rom[0xFF52] & 0xF0);
  word dst =
      0x8000 | (((word)(m_rom[0xFF53] & 0x1F) << 8) | (m_rom[0xFF54] & 0xF0));
  for (int i = 0; i < 16; i++) {
    WriteMemory(dst + i, ReadMemory(src + i));
  }
  src += 16;
  dst += 16;
  m_rom[0xFF51] = (byte)(src >> 8);
  m_rom[0xFF52] = (byte)(src & 0xFF);  // this is okay to just use the lower
                                       // 4-bit turned off value as
  // it is going to be turned of anyway
  word dstOffset = dst - 0x8000;
  m_rom[0xFF53] = (byte)(dstOffset >> 8);
  m_rom[0xFF54] = (byte)(dstOffset & 0xFF);
  m_hdmaRemaining--;
  if (m_hdmaRemaining == 0) {
    m_hdmaActive = false;
    m_rom[0xFF55] = 0xFF;  // 0xFF = transer finished
  }
}
