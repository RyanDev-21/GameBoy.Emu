#include <cstdio>

#include "GameBoy.hpp"

void GameBoy::RequestInterrupt(int id) {
  byte req = ReadMemory(0xFF0F);
  req |= (1 << id);          // merge what ever id position bit
  WriteMemory(0xFF0F, req);  // update the req
}

// Interrupt Bit priority
// Bit 0: V-Blank
// Bit 1:LCD
// Bit 2:Timer
// Bit 3:Joypad
byte GameBoy::DoInterrupts() {
  if (m_MasterInterrupt) {
    byte req = ReadMemory(0xFF0F);
    if (req > 0) {
      byte enabled = ReadMemory(0xFFFF);
      for (int i = 0; i < 5; i++) {
        if ((req & (1 << i)) != 0) {
          if ((enabled & (1 << i)) != 0) {
            ServiceInterrupt(i);
            return 20;
          }
        }
      }
    }
  }
  return 0;
}

// this will reset the master and req register and
// save the pc into the stack
// V-Blank: 0x40
// LCD: 0x48
// TIMER: 0x50
// JOYPAD: 0x60
void GameBoy::ServiceInterrupt(int interrupt) {
  m_Halt = false;
  m_MasterInterrupt = false;
  byte req = ReadMemory(0xFF0F);
  req ^= (1 << interrupt);  // flip the bit
  WriteMemory(0xFF0F, req);

  PushWordToStack(m_programCounter);
  switch (interrupt) {
    case 0: m_programCounter = 0x40; break;
    case 1: m_programCounter = 0x48; break;
    case 2: m_programCounter = 0x50; break;
    case 3: m_programCounter = 0x60; break;
    case 4: m_programCounter = 0x60; break;
  }
}

// Store the data to stack and update the stack pointer
void GameBoy::PushWordToStack(word data) {
  byte high = (data >> 8) & 0xFF;
  byte low = data & 0xFF;

  m_stackPointer.reg--;
  WriteMemory(m_stackPointer.reg, high);
  m_stackPointer.reg--;
  WriteMemory(m_stackPointer.reg, low);
}

word GameBoy::PopWordFromStack() {
  byte low = ReadMemory(m_stackPointer.reg);
  m_stackPointer.reg++;
  byte high = ReadMemory(m_stackPointer.reg);
  m_stackPointer.reg++;
  return (high << 8) | low;
}
