#ifndef GAMEBOY_H
#define GAMEBOY_H

#define FLAG_Z 7;
#define FLAG_N 6;
#define FLAG_H 5;
#define FLAG_C 4;

typedef unsigned char byte;
typedef unsigned short word;

union Register {
    word reg;
    struct {
        byte lo;
        byte hi;
    };
};

class GameBoy {
  private:
    byte m_CartridgeMemory[0x200000];
    byte m_screenData[160][144][3];
    byte m_rom[0x10000];
    Register m_RegisterAF;
    Register m_RegisterBC;
    Register m_RegisterDE;
    Register m_RegisterHL;
    word m_programCounter;
    Register m_stackPointer; // game boy opcode sometimes uses low and high byte
    byte m_ramBanks[0x8000]; // RamBanks for cartridge
    byte current_ramBank;    // by default this is 0
    byte m_enableRAM;        // for  RAM enable
    byte m_enableROM;        // for RAM or ROM
    // by default it has to be 1
    byte current_romBank;

    // ram bank controllers
    byte m_MBU1;
    byte m_MBU2;

    // Handle the switch bank
    void HandleBanking(word address, byte data);
    // enable RAM Banking
    void DoRAMBanking(word address, byte data);
    // Change low ROM Bank
    void DoChangeLoROMBank(byte data);
    // Change high ROM Bank
    void DoChangeHiROMBank(byte data);
    // Change RAM Bank
    void DoChangeRAMBank(byte data);
    // Change  ROM bank
    void DoChangeROMRAMBank(byte data);

  public:
    GameBoy();
    void WriteMemory(word address, byte data);
    byte ReadMemory(word address) const;
    void ReadRom(char const *filePath);
};

#endif
