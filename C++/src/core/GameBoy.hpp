#ifndef GAMEBOY_H
#define GAMEBOY_H

#include <memory>

#include "../utils/types.hpp"
class GameBoy {
 private:
  byte m_CartridgeMemory[0x200000];
  byte m_screenData[144][160][4];
  byte m_ramBanks[0x8000];
  byte m_rom[0x10000];
  byte m_vram[2][0x2000];
  byte m_wram[8][0x1000];
  byte m_bgIndex[160];  // bg/window color index per x
  bool m_bgPrio[160];   // by tile had priority attr and pixel !=0
  TimePoint m_RTCtimeStamp;
  int m_TimerCounter;
  int m_scalineCounter;
  int m_SerialIndex;
  float m_RTCaccumulator;
  word m_DividerCounter;
  Register m_RegisterAF;
  Register m_RegisterBC;
  Register m_RegisterDE;
  Register m_RegisterHL;
  Register m_stackPointer;
  byte current_ramBank;
  byte m_enableRAM;
  byte m_enableROM;
  byte current_romBank;
  byte current_vramBank;
  byte current_wramBank;
  byte m_MBC1;
  byte m_MBC2;
  byte m_MBC3;
  byte m_mbc3RamBankOrRtc;
  byte m_mbc3RtcIdx;
  byte m_RTCWriteState;
  byte m_RTCregs[5];
  byte m_RTCLatch[5];
  byte m_Div;
  byte m_joyPadState;
  byte m_SerialOutput[256];
  bool m_mbc3RtcRegister;
  bool m_RestartDetected;
  bool m_MasterInterrupt;
  bool m_EIpending;
  bool m_Halt;
  bool m_HaltBug;
  bool m_isGBC;
  byte key_1;
  bool m_previousStatusLine;  // for  interrupt

  // for color and stuff
  byte m_BGPalette[0x40];
  byte m_OBJPalette[0x40];
  byte m_BGPaletteIndex;
  byte m_OBJPaletteIndex;
  bool m_autoIncBGPalette;
  bool m_autoIncOBJPalette;
  bool m_doubleSpeed;

  // HDMA (FF51-FF55) state; the source/dest registers live in m_rom[0xFF51-54]
  word m_hdmaRemaining;  // 16-byte chunks left to transfer
  bool m_hdmaActive;
  bool m_hdmaHBlankMode;  // true = HBlank DMA, false = general purpose DMA
  bool m_hdmaLineDone;    // chunk already transferred on this scanline

  void PushWordToStack(word data);
  word PopWordFromStack();
  void ScreenReset();
  void HandleBanking(word address, byte data);
  void DoRAMBanking(word address, byte data);
  byte ReadRTCRegister() const;
  void doChangeRamOrRTC(byte data);
  void handleRTCLatch(byte data);
  void DoChangeLoROMBank(byte data);
  void DoChangeHiROMBank(byte data);
  void DoChangeRAMBank(byte data);
  void DoChangeROMRAMBank(byte data);
  void WriteRTCReg(byte data);
  void DoDividerCounter(int cycles);
  bool TimerClockEnabled() const;
  void tickOneSecond();
  byte GetClockFeq() const;
  void SetClockFeq();
  void RequestInterrupt(int id);
  byte DoInterrupts();
  void ServiceInterrupt(int interrupt);
  bool LCD_enabled();
  void SetLCD_status();
  void DoDMATransfer(byte address);
  void RenderTiles();
  void RenderSprites();
  COLOUR ReadColor(int colorNum, word address);
  GBCcolor ReadColorGBC(int colorNum, byte palette[], byte paletteIdx);
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
  std::unique_ptr<saveData> convertFormat() const;
  void WriteVBK(byte data);   // VRAM bank select
  void WriteSVBK(byte data);  // WRAM bank select
  void WriteBCPS(byte data);  // index
  void WriteBCPD(byte data);  // data
  void WriteOCPS(byte data);  // index
  void WriteOCPD(byte data);  // data
  void DoHDMAChunk();
  void ToggleDoubleSpeed();
  void DrawSpritePixels(int index, bool use8x16);

 public:
  word m_programCounter;
  GameBoy();
  void WriteMemory(word address, byte data);
  byte ReadMemory(word address) const;
  void ReadRom(char const* filePath);
  void UpdateTimers(int cycles);
  int Update();
  void UpdateGraphics(int cycles);
  void DrawScanLine();
  void updateRTC(float secondsElapsed);
  void fastForwardRTC(float secondsElapsed);
  void KeyPressed(int key);
  void KeyReleased(int key);
  const byte* GetScreenData() const;
  void RunTestMode(int maxFrames);
  bool SaveRam(const char* savPath);
  void LoadRam(const char* loadPath);
  word* getBG_Palette() const;
  word* getOBJ_Palette();
  const byte* GetVram(int bank);
};
#endif
