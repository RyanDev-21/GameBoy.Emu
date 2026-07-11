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

// For jump condition
enum CC {
  NZ = 0,
  Z = 1,
  NC = 2,
  C = 3,
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
typedef signed char signed_byte;
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
  byte m_screenData[144][160][4];  //[Height,Width,ARGB]
  byte m_rom[0x10000];
  Register m_RegisterAF;
  Register m_RegisterBC;
  Register m_RegisterDE;
  Register m_RegisterHL;
  word m_programCounter;
  Register m_stackPointer;  // game boy opcode sometimes uses low and high byte
  byte m_ramBanks[0x8000];  // RamBanks for cartridge
  byte current_ramBank;     // by default this is 0
  byte m_enableRAM;         // for  RAM enable
  byte m_enableROM;         // for RAM or ROM
  // by default it has to be 1
  byte current_romBank;

  // ram bank controllers
  byte m_MBU1;
  byte m_MBU2;
  byte m_MBC3;
  byte m_mbc3RamBankOrRtc;
  bool m_mbc3RtcRegister;

  // track the lower bits of 0xFF04
  byte m_DividerCounter;
  // track the last joypad bits
  byte m_joyPadState;
  // Timer Counter
  int m_TimerCounter;

  // scanline count cycles
  int m_scalineCounter;

  byte m_SerialOutput[256];
  int m_SerialIndex;
  word m_PCTrace[64];
  int m_PCTraceIdx;
  bool m_RestartDetected;
  bool m_FirstStart;
  bool m_MasterInterrupt;
  // One delay interrupt
  bool m_EIpending;
  // Halt
  bool m_Halt;
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

  // Opcode Related stuff
  int NextOpCodeExcute();
  int ExcuteOpcode(byte opcode);
  // Opcode translation stuff
  void CPU_8bit_Load(byte& reg);
  void CPU_8bit_Reg_Load(byte& reg1, byte& reg2);
  void CPU_8bit_MemToReg(byte& reg1, Register reg2, OP operation);
  void CPU_8bit_RegToMem(Register reg1, byte reg2, OP operation);
  void CPU_8bit_ImmeToMem(Register reg1);
  void CPU_8bit_ImmeMemToReg(byte& reg1);
  void CPU_8bit_RegToImmeMem(byte reg);
  void CPU_8bit_RegToC(byte reg);
  void CPU_8bit_CToReg(byte& reg);
  void CPU_8bit_ADD(byte& reg, byte toAdd, bool useImmediate, bool addCarry);
  void CPU_8bit_SUB(byte& reg, byte toSub, bool useImmediate, bool borrowCarry);
  void CPU_8bit_XOR(byte& reg, byte toXOR, bool useImmediate);
  void CPU_8bit_AND(byte& reg, byte toAND, bool useImmediate);
  void CPU_8bit_OR(byte& reg, byte toOR, bool useImmediate);
  bool CPU_JUMP_IMMEDIATE(bool condition, int flag, bool useCondition);
  bool CPU_Call(bool condition, int flag, bool useCondition);
  bool CPU_RETURN(bool condition, int flag, bool useCondition);
  void CPU_8bit_RegToImmeN0xFF00(byte reg);
  void CPU_8bit_ImmeN0xFF00ToReg(byte& reg);
  void CPU_16bit_MemToReg(Register& reg);
  void CPU_16bit_Reg_Load(Register& reg1, Register& reg2);
  void CPU_16bit_SPNnToHL();
  void CPU_16bit_RegToImmeMem(Register reg);
  void CPU_16bit_PopToReg(Register& reg);
  void CPU_8bit_CP(byte reg, byte reg1);
  void CPU_8bit_SimOp(byte& reg, OP operation);
  void CPU_16bit_ADD(Register& reg, Register reg2, bool z_flag);
  void CPU_16bit_NToSP();
  void CPU_8bit_SWAP(byte& reg);
  void CPU_8bit_DAA();
  void CPU_8bit_INC(byte& reg, byte& flagReg);
  void CPU_8bit_DEC(byte& reg, byte& flagReg);
  void CPU_8bit_RLC(byte& reg);
  void CPU_8bit_RL(byte& reg);
  void CPU_8bit_RRC(byte& reg);
  void CPU_8bit_RR(byte& reg);
  void CPU_8bit_SLA(byte& reg);
  void CPU_8bit_SRA(byte& reg);
  void CPU_8bit_SRL(byte& reg);
  void CPU_8bit_Bit_Test(byte opcode);
  void CPU_8bit_BIT_SET(byte opcode);
  void CPU_8bit_BIT_RESET(byte reg);
  bool CPU_8bit_JP_2Byte_Imme(CC cc);
  void CPU_8bit_Restart(byte addr);
  // Helpers
  word ReadWord();

 public:
  GameBoy();
  void WriteMemory(word address, byte data);
  byte ReadMemory(word address) const;
  void ReadRom(char const* filePath);
  void UpdateTimers(int cycles);
  int Update();
  void UpdateGraphics(int cycles);
  void DrawScanLine();
  void KeyPressed(int key);
  void KeyReleased(int key);
  const byte* GetScreenData() const;
  word GetPC() const {
    return m_programCounter;
  }
  word GetSP() const {
    return m_stackPointer.reg;
  }
  word GetAF() const {
    return m_RegisterAF.reg;
  }
  word GetBC() const {
    return m_RegisterBC.reg;
  }
  word GetDE() const {
    return m_RegisterDE.reg;
  }
  word GetHL() const {
    return m_RegisterHL.reg;
  }
  const char* GetSerialOutput() const {
    return (const char*)m_SerialOutput;
  }
  bool IsRestartDetected() const {
    return m_RestartDetected;
  }
};

#endif
