#include "./GameBoy.hpp"
#include <cstdio>
#include <cstring>

// Timer address
#define TIMA 0xFF05
#define TMA 0xFF06
#define TMC 0xFF07

// The consturctor will initialize and then set the required state of the emulator as if the real
// game has started
// gameboy doesn't have an isolate stack that's why it is allocated at the near end of higher byte
// after the interept slot 0xFFFF
GameBoy::GameBoy()
    :

      m_programCounter(0x100), m_RegisterAF{.reg = 0x01B0}, m_RegisterBC{.reg = 0x0013},
      m_RegisterDE{.reg = 0x00D8}, m_RegisterHL{.reg = 0x014D}, m_stackPointer{.reg = 0xFFFE},
      current_romBank(1), current_ramBank(0), m_MBU1(false), m_MBU2(false), m_enableRAM(false) {
    // init the ramBanks
    memset(&m_ramBanks, 0, sizeof(m_ramBanks));
    // Memmory Controller
    // set the ram bank
    switch (m_CartridgeMemory[0x147]) {
    case 1:
        m_MBU1 = true;
        break;
    case 2:
        m_MBU1 = true;
        break;
    case 3:
        m_MBU1 = true;
        break;
    case 5:
        m_MBU2 = true;
        break;
    case 6:
        m_MBU2 = true;
        break;

    default:
        break;
    }

    m_rom[0xFF05] = 0x00; // TIMA
    m_rom[0xFF06] = 0x00; // TMA
    m_rom[0xFF07] = 0x00; // TAC
    m_rom[0xFF10] = 0x80; // NR10
    m_rom[0xFF11] = 0xBF; // NR11
    m_rom[0xFF12] = 0xF3; // NR12
    m_rom[0xFF14] = 0xBF; // NR14
    m_rom[0xFF16] = 0x3F; // NR21
    m_rom[0xFF17] = 0x00; // NR22
    m_rom[0xFF19] = 0xBF; // NE24
    m_rom[0xFF1A] = 0x7F; // NR30
    m_rom[0xFF1B] = 0xFF; // NR31
    m_rom[0xFF1C] = 0x9F; // NR32
    m_rom[0xFF1E] = 0xBF; // NR33
    m_rom[0xFF20] = 0xFF; // NR41
    m_rom[0xFF21] = 0x00; // NR42
    m_rom[0xFF22] = 0x00; // NR43
    m_rom[0xFF23] = 0xBF; // NR30
    m_rom[0xFF24] = 0x77; // NR50
    m_rom[0xFF25] = 0xF3; // NR51
    m_rom[0xFF26] = 0xF1; // NR52
    m_rom[0xFF42] = 0x00; // LCDC
    m_rom[0xFF43] = 0x00; // SCY
    m_rom[0xFF40] = 0x91; // SCX
    m_rom[0xFF45] = 0x00; // LYC
    m_rom[0xFF47] = 0xFC; // BGP
    m_rom[0xFF48] = 0xFF; // OBP0
    m_rom[0xFF49] = 0xFF; // OBP1
    m_rom[0xFF4A] = 0x00; // WY
    m_rom[0xFF4B] = 0x00; // WX
    m_rom[0xFFFF] = 0x00; // IE
}

void GameBoy::WriteMemory(word address, byte data) {
    // if within the switching range then handle the switching
    if (address < 0x8000) {
        HandleBanking(address, data);
    }
    // if writing to the dividerRegister
    else if (address == 0xFF04) {
        m_rom[0xFF04] = 0; // reset to 0
    }

    // if writing to TMC
    else if (address == TMC) {
        byte currentFeq = GetClockFeq();
        m_rom[TMC] = data; // this can change the enable bit or feq bits
        byte newFeq = GetClockFeq();
        if (currentFeq != newFeq) { //  change in feq
            SetClockFeq();          // set new one
        }
    }

    // if within the switchable ram range
    else if (address >= 0xA000 && address <= 0xC000) {
        if (m_enableRAM) {
            word new_addr = address - 0xA000;
            m_CartridgeMemory[new_addr + (current_ramBank * 0x2000)] = data;
        }
    }

    // writing in echo region
    else if ((address >= 0xE000) && (address < 0xFE00)) {
        m_rom[address] = data;
        WriteMemory(address - 0x2000, data); // has to write into wram space too
    }
    // no write allowed for these regions
    else if (address >= 0xFEA0 && address < 0xFEFF) {
    }
    // others
    else {
        m_rom[address] = data;
    }
};

void GameBoy::ReadRom(char const *filePath) {
    memset(&m_CartridgeMemory, 0, sizeof(m_CartridgeMemory));
    FILE *in;

    in = fopen(filePath, "rb");
    fread(m_CartridgeMemory, 1, 0x200000, in);
    fclose(in);
};

byte GameBoy::ReadMemory(word address) const {
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
    // others region? return
    else {
        return m_rom[address];
    }
};

void GameBoy::HandleBanking(word address, byte data) {
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
    byte low5 = data & 31; // lower 5 bits
    // 224 in binary = 1110 0000
    current_romBank &= 224;  // turn of lower 5 bit
    current_romBank |= low5; // merge
    if (current_romBank == 0) {
        current_romBank++;
    }
}

void GameBoy::DoChangeHiROMBank(byte data) {
    current_romBank &= 31;       // take out 5 bit
    byte new_data = data & 224;  // take out 3 bit
    current_romBank |= new_data; // merge
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
void GameBoy::DoChangeRAMBank(byte data) { current_ramBank = data & 3; }

void GameBoy::DoChangeROMRAMBank(byte data) {
    byte new_data = data & 0x1; // take out 1 bit
    m_enableROM = (new_data == 0) ? true : false;
    // reset  ramBank to 0
    if (m_enableROM) {
        current_ramBank = 0;
    }
}

void GameBoy::UpdateTimers(int cycles) {
    // update the divider register
    DoDividerCounter(cycles);
    // only if the timer clock is enabled
    if (TimerClockEnabled()) {
        m_TimerCounter -= cycles;
        if (m_TimerCounter <= 0) {         // when finished
            SetClockFeq();                 // reset timer counter
            if (ReadMemory(TIMA) == 255) { // time about to overflow
                // TMA value stores the starting count for interrupt
                WriteMemory(TIMA, ReadMemory(TMA)); // reset with TMA value
                // requestInterrupt(2);
            } else {
                WriteMemory(TIMA, ReadMemory(TIMA) + 1); // plus the current timer
            }
        }
    }
};

// track the divider reigster
void GameBoy::DoDividerCounter(int cycles) {
    m_DividerCounter += cycles;
    if (m_DividerCounter >= 255) {
        m_DividerCounter = 0; // reset
        m_rom[0xFF04]++;      // increase the divider register
    }
}
// Check timer clock enabled
// 3rd bit of the TMC return the bool for clock enable
// 0 = disabled
// 1 = enabled
bool GameBoy::TimerClockEnabled() const {
    bool status = ((ReadMemory(TMC) & 4) == 0 ? false : true);
    return status;
}

// Last two bit of TMC gives clock feq
byte GameBoy::GetClockFeq() const { return ReadMemory(TMC) & 3; }

// ClockSpeed of GameBoy is 4,194,304
// timerCounter = ClockSpeed/feq
void GameBoy::SetClockFeq() {
    byte feq = GetClockFeq();

    switch (feq) {
    case 0:                    // 00
        m_TimerCounter = 1024; // feq 4096
        break;
    case 1:                  // 01
        m_TimerCounter = 16; // feq 262,144
        break;
    case 2:                  // 10
        m_TimerCounter = 64; // feq 65536
        break;
    case 3:                   // 11
        m_TimerCounter = 256; // feq 16,382
        break;
    }
}
