#ifndef GAMEBOY_H
#define GAMEBOY_H

#include "../utils/types.hpp"

class GameBoy {
 private:
  byte m_CartridgeMemory[0x200000];
  byte m_screenData[144][160][4];
  byte m_rom[0x10000];
  Register m_RegisterAF;
  Register m_RegisterBC;
  Register m_RegisterDE;
  Register m_RegisterHL;
  word m_programCounter;
  Register m_stackPointer;
  byte m_ramBanks[0x8000];
  byte current_ramBank;
  byte m_enableRAM;
  byte m_enableROM;
  byte current_romBank;

  byte m_MBU1;
  byte m_MBU2;
  byte m_MBC3;
  byte m_mbc3RamBankOrRtc;
  bool m_mbc3RtcRegister;

  byte m_DividerCounter;
  byte m_joyPadState;
  int m_TimerCounter;
  int m_scalineCounter;

  byte m_SerialOutput[256];
  int m_SerialIndex;
  bool m_RestartDetected;
  bool m_MasterInterrupt;
  bool m_EIpending;
  bool m_Halt;

  void PushWordToStack(word data);
  word PopWordFromStack();
  void ScreenReset();
  void HandleBanking(word address, byte data);
  void DoRAMBanking(word address, byte data);
  byte ReadRTCRegister(byte value);
  void DoChangeLoROMBank(byte data);
  void DoChangeHiROMBank(byte data);
  void DoChangeRAMBank(byte data);
  void DoChangeROMRAMBank(byte data);
  void DoDividerCounter(int cycles);
  bool TimerClockEnabled() const;
  byte GetClockFeq() const;
  void SetClockFeq();
  void RequestInterrupt(int id);
  void DoInterrupts();
  void ServiceInterrupt(int interrupt);
  bool LCD_enabled();
  void SetLCD_status();
  void DoDMATransfer(byte address);
  void RenderTiles();
  void RenderSprites();
  COLOUR ReadColor(int colorNum, word address);
  byte GetJoyPadState() const;
  int NextOpCodeExcute();
  int ExcuteOpcode(byte opcode);
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
  void RunTestMode(int maxFrames);
  bool SaveRam(const char* savPath);
  void LoadRam(const char* loadPath);
};

#endif
