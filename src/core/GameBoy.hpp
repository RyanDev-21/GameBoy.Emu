#ifndef GAMEBOY_H
#define GAMEBOY_H

// For flag Status
#define FLAG_Z 7
#define FLAG_N 6
#define FLAG_H 5
#define FLAG_C 4
// Color Palette
enum COLOUR {
    WHITE = 0,
    LIGHT_GRAY = 1,
    DARK_GRAY = 2,
    BLACK = 3,
};

// Some quirks to handle the opcode
// Not Important
enum OP {
    NONE = 1,
    INC = 2,
    DEC = 3,
};

typedef unsigned char byte;
typedef unsigned short word;
typedef signed short signed_word;

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
    byte m_screenData[160][144][3]; //[Width,Height,Color]
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

    // track the lower bits of 0xFF04
    byte m_DividerCounter;
    // track the last joypad bits
    byte m_joyPadState;
    // Timer Counter
    int m_TimerCounter;

    // scanline count cycles
    int m_scalineCounter;

    // Global Interrupt
    bool m_MasterInterrupt;

    // Stack Related
    void PushWordToStack(word data);
    word PopWordFromStack();
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
    // Plus the divider register(use as random value and stuff)
    void DoDividerCounter(int cycles);
    // Timer Related functions
    bool TimerClockEnabled() const;
    byte GetClockFeq() const;
    void SetClockFeq();

    // Interupt
    void RequestInterrupt(int id);
    void DoInterrupts();
    // server interrupt req
    void ServiceInterrupt(int interrupt);
    bool LCD_enabled();
    void SetLCD_status();
    // DMA transfer for sprite ram
    void DoDMATransfer(byte address);
    // Render The background && sprites
    void RenderTiles();
    void RenderSprites();
    // Color Related Funcs
    COLOUR ReadColor(int colorNum, word address);
    // Get Current joypad State
    byte GetJoyPadState() const;
    void KeyPressed(int key);
    void KeyReleased(int key);

    // Opcode Related stuff
    void NextOpCodeExcute();
    int ExcuteOpcode(byte opcode);
    // Opcode translation stuff
    void CPU_8bit_Load(byte &reg);
    void CPU_8bit_Reg_Load(byte &reg1, byte &reg2);
    void CPU_8bit_MemToReg(byte &reg1, Register reg2, OP operation);
    void CPU_8bit_RegToMem(Register reg1, byte reg2, OP operation);
    void CPU_8bit_ImmeToMem(Register reg1);
    void CPU_16bit_MemToReg(byte &reg1);
    void CPU_16bit_RegToMem(byte reg);
    void CPU_8bit_RegToC(byte reg);
    void CPU_8bit_CToReg(byte reg);
    void CPU_8bit_ADD(byte &reg, byte toAdd, bool useImmediate, bool addCarry);
    void CPU_8bit_SUB(byte &reg, byte toSub, bool useImmediate, bool borrowCarry);
    void CPU_8bit_XOR(byte &reg, byte toXOR, bool useImmediate);
    void CPU_8bit_AND(byte &reg, byte toAND, bool useImmediate);
    void CPU_8bit_OR(byte &reg, byte toOR, bool useImmediate);
    void CPU_JUMP_IMMEDIATE(bool condition, int flag, bool useCondition);
    void CPU_Call(bool condition, int flag, bool useCondition);
    void CPU_RETURN(bool condition, int flag, bool useCondition);
    // Helpers
    word ReadWord();

  public:
    GameBoy();
    void WriteMemory(word address, byte data);
    byte ReadMemory(word address) const;
    void ReadRom(char const *filePath);
    void UpdateTimers(int cycles);
    void Update();
    void UpdateGraphics(int cycles);
    void DrawScanLine();
};

#endif
