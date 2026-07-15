#include <cstring>

#include "GameBoy.hpp"

int GameBoy::NextOpCodeExcute() {
  int res = 0;
  if (m_Halt) {
    if (m_EIpending) {
      m_EIpending = false;
      m_MasterInterrupt = true;
    }
    if (!m_MasterInterrupt) {
      byte req = ReadMemory(0xFF0F);
      byte enabled = ReadMemory(0xFFFF);
      if ((req & enabled) != 0) {
        m_Halt = false;
      }
    }
    return 4;
  }

  if (m_EIpending) {
    m_EIpending = false;
    m_MasterInterrupt = true;
  }

  byte opcode = ReadMemory(m_programCounter);
  m_programCounter++;
  res = ExcuteOpcode(opcode);

  return res;
}

int GameBoy::ExcuteOpcode(byte opcode) {
  switch (opcode) {
    case 0x00: return 4;
    case 0x01: CPU_16bit_MemToReg(m_RegisterBC); return 12;
    case 0x02: CPU_8bit_RegToMem(m_RegisterBC, m_RegisterAF.hi, NONE); return 8;
    case 0x03: m_RegisterBC.reg++; return 8;
    case 0x04: CPU_8bit_SimOp(m_RegisterBC.hi, INC); return 4;
    case 0x05: CPU_8bit_SimOp(m_RegisterBC.hi, DEC); return 4;
    case 0x06: CPU_8bit_Load(m_RegisterBC.hi); return 8;
    case 0x07:
      CPU_8bit_RLC(m_RegisterAF.hi);
      m_RegisterAF.lo &= ~(1 << FLAG_Z);
      return 4;
    case 0x08: CPU_16bit_RegToImmeMem(m_stackPointer); return 20;
    case 0x09: CPU_16bit_ADD(m_RegisterHL, m_RegisterBC, true); return 8;
    case 0x0A: CPU_8bit_MemToReg(m_RegisterAF.hi, m_RegisterBC, NONE); return 8;
    case 0x0B: m_RegisterBC.reg--; return 8;
    case 0x0C: CPU_8bit_SimOp(m_RegisterBC.lo, INC); return 4;
    case 0x0D: CPU_8bit_SimOp(m_RegisterBC.lo, DEC); return 4;
    case 0x0E: CPU_8bit_Load(m_RegisterBC.lo); return 8;
    case 0x0F:
      CPU_8bit_RRC(m_RegisterAF.hi);
      m_RegisterAF.lo &= ~(1 << FLAG_Z);
      return 4;
    case 0x10: m_programCounter++; return 4;
    case 0x11: CPU_16bit_MemToReg(m_RegisterDE); return 12;
    case 0x12: CPU_8bit_RegToMem(m_RegisterDE, m_RegisterAF.hi, NONE); return 8;
    case 0x13: m_RegisterDE.reg++; return 8;
    case 0x14: CPU_8bit_SimOp(m_RegisterDE.hi, INC); return 4;
    case 0x15: CPU_8bit_SimOp(m_RegisterDE.hi, DEC); return 4;
    case 0x16: CPU_8bit_Load(m_RegisterDE.hi); return 8;
    case 0x17:
      CPU_8bit_RL(m_RegisterAF.hi);
      m_RegisterAF.lo &= ~(1 << FLAG_Z);
      return 4;
    case 0x1A: CPU_8bit_MemToReg(m_RegisterAF.hi, m_RegisterDE, NONE); return 8;
    case 0x1B: m_RegisterDE.reg--; return 8;
    case 0x1C: CPU_8bit_SimOp(m_RegisterDE.lo, INC); return 4;
    case 0x1D: CPU_8bit_SimOp(m_RegisterDE.lo, DEC); return 4;
    case 0x1E: CPU_8bit_Load(m_RegisterDE.lo); return 8;
    case 0x1F:
      CPU_8bit_RR(m_RegisterAF.hi);
      m_RegisterAF.lo &= ~(1 << FLAG_Z);
      return 4;
    case 0x19: CPU_16bit_ADD(m_RegisterHL, m_RegisterDE, true); return 8;
    case 0x21: CPU_16bit_MemToReg(m_RegisterHL); return 12;
    case 0x22: CPU_8bit_RegToMem(m_RegisterHL, m_RegisterAF.hi, INC); return 8;
    case 0x23: m_RegisterHL.reg++; return 8;
    case 0x24: CPU_8bit_SimOp(m_RegisterHL.hi, INC); return 4;
    case 0x25: CPU_8bit_SimOp(m_RegisterHL.hi, DEC); return 4;
    case 0x26: CPU_8bit_Load(m_RegisterHL.hi); return 8;
    case 0x27: CPU_8bit_DAA(); return 4;
    case 0x2A: CPU_8bit_MemToReg(m_RegisterAF.hi, m_RegisterHL, INC); return 8;
    case 0x2B: m_RegisterHL.reg--; return 8;
    case 0x2C: CPU_8bit_SimOp(m_RegisterHL.lo, INC); return 4;
    case 0x2D: CPU_8bit_SimOp(m_RegisterHL.lo, DEC); return 4;
    case 0x2E: CPU_8bit_Load(m_RegisterHL.lo); return 8;
    case 0x2F:
      m_RegisterAF.hi ^= 0xFF;
      m_RegisterAF.lo |= (1 << FLAG_N) | (1 << FLAG_H);
      return 4;
    case 0x29: CPU_16bit_ADD(m_RegisterHL, m_RegisterHL, true); return 8;
    case 0x31: CPU_16bit_MemToReg(m_stackPointer); return 12;
    case 0x32: CPU_8bit_RegToMem(m_RegisterHL, m_RegisterAF.hi, DEC); return 8;
    case 0x33: m_stackPointer.reg++; return 8;
    case 0x34: {
      byte val = ReadMemory(m_RegisterHL.reg);
      CPU_8bit_SimOp(val, INC);
      WriteMemory(m_RegisterHL.reg, val);
      return 12;
    }
    case 0x35: {
      byte val = ReadMemory(m_RegisterHL.reg);
      CPU_8bit_SimOp(val, DEC);
      WriteMemory(m_RegisterHL.reg, val);
      return 12;
    }
    case 0x3A: CPU_8bit_MemToReg(m_RegisterAF.hi, m_RegisterHL, DEC); return 8;
    case 0x3B: m_stackPointer.reg--; return 8;
    case 0x3C: CPU_8bit_SimOp(m_RegisterAF.hi, INC); return 4;
    case 0x3D: CPU_8bit_SimOp(m_RegisterAF.hi, DEC); return 4;
    case 0x3E: CPU_8bit_Load(m_RegisterAF.hi); return 8;
    case 0x39: CPU_16bit_ADD(m_RegisterHL, m_stackPointer, true); return 8;
    case 0x7F: CPU_8bit_Reg_Load(m_RegisterAF.hi, m_RegisterAF.hi); return 4;
    case 0x77: CPU_8bit_RegToMem(m_RegisterHL, m_RegisterAF.hi, NONE); return 8;
    case 0x78: CPU_8bit_Reg_Load(m_RegisterAF.hi, m_RegisterBC.hi); return 4;
    case 0x79: CPU_8bit_Reg_Load(m_RegisterAF.hi, m_RegisterBC.lo); return 4;
    case 0x7A: CPU_8bit_Reg_Load(m_RegisterAF.hi, m_RegisterDE.hi); return 4;
    case 0x7B: CPU_8bit_Reg_Load(m_RegisterAF.hi, m_RegisterDE.lo); return 4;
    case 0x7C: CPU_8bit_Reg_Load(m_RegisterAF.hi, m_RegisterHL.hi); return 4;
    case 0x7D: CPU_8bit_Reg_Load(m_RegisterAF.hi, m_RegisterHL.lo); return 4;
    case 0x7E: CPU_8bit_MemToReg(m_RegisterAF.hi, m_RegisterHL, NONE); return 8;
    case 0x40: CPU_8bit_Reg_Load(m_RegisterBC.hi, m_RegisterBC.hi); return 4;
    case 0x41: CPU_8bit_Reg_Load(m_RegisterBC.hi, m_RegisterBC.lo); return 4;
    case 0x42: CPU_8bit_Reg_Load(m_RegisterBC.hi, m_RegisterDE.hi); return 4;
    case 0x43: CPU_8bit_Reg_Load(m_RegisterBC.hi, m_RegisterDE.lo); return 4;
    case 0x44: CPU_8bit_Reg_Load(m_RegisterBC.hi, m_RegisterHL.hi); return 4;
    case 0x45: CPU_8bit_Reg_Load(m_RegisterBC.hi, m_RegisterHL.lo); return 4;
    case 0x46: CPU_8bit_MemToReg(m_RegisterBC.hi, m_RegisterHL, NONE); return 8;
    case 0x47: CPU_8bit_Reg_Load(m_RegisterBC.hi, m_RegisterAF.hi); return 4;
    case 0x48: CPU_8bit_Reg_Load(m_RegisterBC.lo, m_RegisterBC.hi); return 4;
    case 0x49: CPU_8bit_Reg_Load(m_RegisterBC.lo, m_RegisterBC.lo); return 4;
    case 0x4A: CPU_8bit_Reg_Load(m_RegisterBC.lo, m_RegisterDE.hi); return 4;
    case 0x4B: CPU_8bit_Reg_Load(m_RegisterBC.lo, m_RegisterDE.lo); return 4;
    case 0x4C: CPU_8bit_Reg_Load(m_RegisterBC.lo, m_RegisterHL.hi); return 4;
    case 0x4D: CPU_8bit_Reg_Load(m_RegisterBC.lo, m_RegisterHL.lo); return 4;
    case 0x4E: CPU_8bit_MemToReg(m_RegisterBC.lo, m_RegisterHL, NONE); return 8;
    case 0x4F: CPU_8bit_Reg_Load(m_RegisterBC.lo, m_RegisterAF.hi); return 4;
    case 0x50: CPU_8bit_Reg_Load(m_RegisterDE.hi, m_RegisterBC.hi); return 4;
    case 0x51: CPU_8bit_Reg_Load(m_RegisterDE.hi, m_RegisterBC.lo); return 4;
    case 0x52: CPU_8bit_Reg_Load(m_RegisterDE.hi, m_RegisterDE.hi); return 4;
    case 0x53: CPU_8bit_Reg_Load(m_RegisterDE.hi, m_RegisterDE.lo); return 4;
    case 0x54: CPU_8bit_Reg_Load(m_RegisterDE.hi, m_RegisterHL.hi); return 4;
    case 0x55: CPU_8bit_Reg_Load(m_RegisterDE.hi, m_RegisterHL.lo); return 4;
    case 0x56: CPU_8bit_MemToReg(m_RegisterDE.hi, m_RegisterHL, NONE); return 8;
    case 0x57: CPU_8bit_Reg_Load(m_RegisterDE.hi, m_RegisterAF.hi); return 4;
    case 0x58: CPU_8bit_Reg_Load(m_RegisterDE.lo, m_RegisterBC.hi); return 4;
    case 0x59: CPU_8bit_Reg_Load(m_RegisterDE.lo, m_RegisterBC.lo); return 4;
    case 0x5A: CPU_8bit_Reg_Load(m_RegisterDE.lo, m_RegisterDE.hi); return 4;
    case 0x5B: CPU_8bit_Reg_Load(m_RegisterDE.lo, m_RegisterDE.lo); return 4;
    case 0x5C: CPU_8bit_Reg_Load(m_RegisterDE.lo, m_RegisterHL.hi); return 4;
    case 0x5D: CPU_8bit_Reg_Load(m_RegisterDE.lo, m_RegisterHL.lo); return 4;
    case 0x5E: CPU_8bit_MemToReg(m_RegisterDE.lo, m_RegisterHL, NONE); return 8;
    case 0x5F: CPU_8bit_Reg_Load(m_RegisterDE.lo, m_RegisterAF.hi); return 4;
    case 0x60: CPU_8bit_Reg_Load(m_RegisterHL.hi, m_RegisterBC.hi); return 4;
    case 0x61: CPU_8bit_Reg_Load(m_RegisterHL.hi, m_RegisterBC.lo); return 4;
    case 0x62: CPU_8bit_Reg_Load(m_RegisterHL.hi, m_RegisterDE.hi); return 4;
    case 0x63: CPU_8bit_Reg_Load(m_RegisterHL.hi, m_RegisterDE.lo); return 4;
    case 0x64: CPU_8bit_Reg_Load(m_RegisterHL.hi, m_RegisterHL.hi); return 4;
    case 0x65: CPU_8bit_Reg_Load(m_RegisterHL.hi, m_RegisterHL.lo); return 4;
    case 0x66: CPU_8bit_MemToReg(m_RegisterHL.hi, m_RegisterHL, NONE); return 8;
    case 0x67: CPU_8bit_Reg_Load(m_RegisterHL.hi, m_RegisterAF.hi); return 4;
    case 0x68: CPU_8bit_Reg_Load(m_RegisterHL.lo, m_RegisterBC.hi); return 4;
    case 0x69: CPU_8bit_Reg_Load(m_RegisterHL.lo, m_RegisterBC.lo); return 4;
    case 0x6A: CPU_8bit_Reg_Load(m_RegisterHL.lo, m_RegisterDE.hi); return 4;
    case 0x6B: CPU_8bit_Reg_Load(m_RegisterHL.lo, m_RegisterDE.lo); return 4;
    case 0x6C: CPU_8bit_Reg_Load(m_RegisterHL.lo, m_RegisterHL.hi); return 4;
    case 0x6D: CPU_8bit_Reg_Load(m_RegisterHL.lo, m_RegisterHL.lo); return 4;
    case 0x6E: CPU_8bit_MemToReg(m_RegisterHL.lo, m_RegisterHL, NONE); return 8;
    case 0x6F: CPU_8bit_Reg_Load(m_RegisterHL.lo, m_RegisterAF.hi); return 4;

    case 0x70: CPU_8bit_RegToMem(m_RegisterHL, m_RegisterBC.hi, NONE); return 8;
    case 0x71: CPU_8bit_RegToMem(m_RegisterHL, m_RegisterBC.lo, NONE); return 8;
    case 0x72: CPU_8bit_RegToMem(m_RegisterHL, m_RegisterDE.hi, NONE); return 8;
    case 0x73: CPU_8bit_RegToMem(m_RegisterHL, m_RegisterDE.lo, NONE); return 8;
    case 0x74: CPU_8bit_RegToMem(m_RegisterHL, m_RegisterHL.hi, NONE); return 8;
    case 0x75: CPU_8bit_RegToMem(m_RegisterHL, m_RegisterHL.lo, NONE); return 8;
    case 0x76: {
      if (!m_MasterInterrupt && !m_EIpending) {
        byte req = ReadMemory(0xFF0F);
        byte enabled = ReadMemory(0xFFFF);
        if ((req & enabled) != 0) {
          return 4;
        }
      }
      m_Halt = true;
      return 4;
    }
    case 0x36: CPU_8bit_ImmeToMem(m_RegisterHL); return 12;

    case 0x80:
      CPU_8bit_ADD(m_RegisterAF.hi, m_RegisterBC.hi, false, false);
      return 4;
    case 0x81:
      CPU_8bit_ADD(m_RegisterAF.hi, m_RegisterBC.lo, false, false);
      return 4;
    case 0x82:
      CPU_8bit_ADD(m_RegisterAF.hi, m_RegisterDE.hi, false, false);
      return 4;
    case 0x83:
      CPU_8bit_ADD(m_RegisterAF.hi, m_RegisterDE.lo, false, false);
      return 4;
    case 0x84:
      CPU_8bit_ADD(m_RegisterAF.hi, m_RegisterHL.hi, false, false);
      return 4;
    case 0x85:
      CPU_8bit_ADD(m_RegisterAF.hi, m_RegisterHL.lo, false, false);
      return 4;
    case 0x86: {
      word nn = ReadMemory(m_RegisterHL.reg);
      CPU_8bit_ADD(m_RegisterAF.hi, nn, false, false);
      return 8;
    }
    case 0x87:
      CPU_8bit_ADD(m_RegisterAF.hi, m_RegisterAF.hi, false, false);
      return 4;
    case 0x88:
      CPU_8bit_ADD(m_RegisterAF.hi, m_RegisterBC.hi, false, true);
      return 4;
    case 0x89:
      CPU_8bit_ADD(m_RegisterAF.hi, m_RegisterBC.lo, false, true);
      return 4;
    case 0x8A:
      CPU_8bit_ADD(m_RegisterAF.hi, m_RegisterDE.hi, false, true);
      return 4;
    case 0x8B:
      CPU_8bit_ADD(m_RegisterAF.hi, m_RegisterDE.lo, false, true);
      return 4;
    case 0x8C:
      CPU_8bit_ADD(m_RegisterAF.hi, m_RegisterHL.hi, false, true);
      return 4;
    case 0x8D:
      CPU_8bit_ADD(m_RegisterAF.hi, m_RegisterHL.lo, false, true);
      return 4;
    case 0x8E: {
      word nn = ReadMemory(m_RegisterHL.reg);
      CPU_8bit_ADD(m_RegisterAF.hi, nn, false, true);
      return 8;
    }

    case 0x8F:
      CPU_8bit_ADD(m_RegisterAF.hi, m_RegisterAF.hi, false, true);
      return 4;
    case 0x90:
      CPU_8bit_SUB(m_RegisterAF.hi, m_RegisterBC.hi, false, false);
      return 4;
    case 0x91:
      CPU_8bit_SUB(m_RegisterAF.hi, m_RegisterBC.lo, false, false);
      return 4;
    case 0x92:
      CPU_8bit_SUB(m_RegisterAF.hi, m_RegisterDE.hi, false, false);
      return 4;
    case 0x93:
      CPU_8bit_SUB(m_RegisterAF.hi, m_RegisterDE.lo, false, false);
      return 4;
    case 0x94:
      CPU_8bit_SUB(m_RegisterAF.hi, m_RegisterHL.hi, false, false);
      return 4;
    case 0x95:
      CPU_8bit_SUB(m_RegisterAF.hi, m_RegisterHL.lo, false, false);
      return 4;
    case 0x96: {
      byte nn = ReadMemory(m_RegisterHL.reg);
      CPU_8bit_SUB(m_RegisterAF.hi, nn, false, false);
      return 8;
    }
    case 0x97:
      CPU_8bit_SUB(m_RegisterAF.hi, m_RegisterAF.hi, false, false);
      return 4;

    case 0x98:
      CPU_8bit_SUB(m_RegisterAF.hi, m_RegisterBC.hi, false, true);
      return 4;
    case 0x99:
      CPU_8bit_SUB(m_RegisterAF.hi, m_RegisterBC.lo, false, true);
      return 4;
    case 0x9A:
      CPU_8bit_SUB(m_RegisterAF.hi, m_RegisterDE.hi, false, true);
      return 4;
    case 0x9B:
      CPU_8bit_SUB(m_RegisterAF.hi, m_RegisterDE.lo, false, true);
      return 4;
    case 0x9C:
      CPU_8bit_SUB(m_RegisterAF.hi, m_RegisterHL.hi, false, true);
      return 4;
    case 0x9D:
      CPU_8bit_SUB(m_RegisterAF.hi, m_RegisterHL.lo, false, true);
      return 4;
    case 0x9E: {
      byte nn = ReadMemory(m_RegisterHL.reg);
      CPU_8bit_SUB(m_RegisterAF.hi, nn, false, true);
      return 8;
    }
    case 0x9F:
      CPU_8bit_SUB(m_RegisterAF.hi, m_RegisterAF.hi, false, true);
      return 4;

    case 0xA0: CPU_8bit_AND(m_RegisterAF.hi, m_RegisterBC.hi, false); return 4;
    case 0xA1: CPU_8bit_AND(m_RegisterAF.hi, m_RegisterBC.lo, false); return 4;
    case 0xA2: CPU_8bit_AND(m_RegisterAF.hi, m_RegisterDE.hi, false); return 4;
    case 0xA3: CPU_8bit_AND(m_RegisterAF.hi, m_RegisterDE.lo, false); return 4;
    case 0xA4: CPU_8bit_AND(m_RegisterAF.hi, m_RegisterHL.hi, false); return 4;
    case 0xA5: CPU_8bit_AND(m_RegisterAF.hi, m_RegisterHL.lo, false); return 4;
    case 0xA6: {
      byte nn = ReadMemory(m_RegisterHL.reg);
      CPU_8bit_AND(m_RegisterAF.hi, nn, false);
      return 8;
    }
    case 0xA7: CPU_8bit_AND(m_RegisterAF.hi, m_RegisterAF.hi, false); return 4;

    case 0xA8: CPU_8bit_XOR(m_RegisterAF.hi, m_RegisterBC.hi, false); return 4;
    case 0xA9: CPU_8bit_XOR(m_RegisterAF.hi, m_RegisterBC.lo, false); return 4;
    case 0xAA: CPU_8bit_XOR(m_RegisterAF.hi, m_RegisterDE.hi, false); return 4;
    case 0xAB: CPU_8bit_XOR(m_RegisterAF.hi, m_RegisterDE.lo, false); return 4;
    case 0xAC: CPU_8bit_XOR(m_RegisterAF.hi, m_RegisterHL.hi, false); return 4;
    case 0xAD: CPU_8bit_XOR(m_RegisterAF.hi, m_RegisterHL.lo, false); return 4;
    case 0xAE: {
      byte nn = ReadMemory(m_RegisterHL.reg);
      CPU_8bit_XOR(m_RegisterAF.hi, nn, false);
      return 8;
    }
    case 0xAF: CPU_8bit_XOR(m_RegisterAF.hi, m_RegisterAF.hi, false); return 4;

    case 0xEE: CPU_8bit_XOR(m_RegisterAF.hi, 0, true); return 8;

    case 0xB0: CPU_8bit_OR(m_RegisterAF.hi, m_RegisterBC.hi, false); return 4;
    case 0xB1: CPU_8bit_OR(m_RegisterAF.hi, m_RegisterBC.lo, false); return 4;
    case 0xB2: CPU_8bit_OR(m_RegisterAF.hi, m_RegisterDE.hi, false); return 4;
    case 0xB3: CPU_8bit_OR(m_RegisterAF.hi, m_RegisterDE.lo, false); return 4;
    case 0xB4: CPU_8bit_OR(m_RegisterAF.hi, m_RegisterHL.hi, false); return 4;
    case 0xB5: CPU_8bit_OR(m_RegisterAF.hi, m_RegisterHL.lo, false); return 4;
    case 0xB6: {
      byte nn = ReadMemory(m_RegisterHL.reg);
      CPU_8bit_OR(m_RegisterAF.hi, nn, false);
      return 8;
    }
    case 0xB7: CPU_8bit_OR(m_RegisterAF.hi, m_RegisterAF.hi, false); return 4;

    case 0xB8: CPU_8bit_CP(m_RegisterAF.hi, m_RegisterBC.hi); return 4;
    case 0xB9: CPU_8bit_CP(m_RegisterAF.hi, m_RegisterBC.lo); return 4;
    case 0xBA: CPU_8bit_CP(m_RegisterAF.hi, m_RegisterDE.hi); return 4;
    case 0xBB: CPU_8bit_CP(m_RegisterAF.hi, m_RegisterDE.lo); return 4;
    case 0xBC: CPU_8bit_CP(m_RegisterAF.hi, m_RegisterHL.hi); return 4;
    case 0xBD: CPU_8bit_CP(m_RegisterAF.hi, m_RegisterHL.lo); return 4;
    case 0xBE: {
      byte nn = ReadMemory(m_RegisterHL.reg);
      CPU_8bit_CP(m_RegisterAF.hi, nn);
      return 8;
    }
    case 0xBF: CPU_8bit_CP(m_RegisterAF.hi, m_RegisterAF.hi); return 4;

    case 0xFE: {
      byte nn = ReadMemory(m_programCounter);
      m_programCounter++;
      CPU_8bit_CP(m_RegisterAF.hi, nn);
      return 8;
    }

    case 0x18: CPU_JUMP_IMMEDIATE(false, 0, false); return 12;
    case 0x20: return CPU_JUMP_IMMEDIATE(false, FLAG_Z, true) ? 12 : 8;
    case 0x28: return CPU_JUMP_IMMEDIATE(true, FLAG_Z, true) ? 12 : 8;
    case 0x30: return CPU_JUMP_IMMEDIATE(false, FLAG_C, true) ? 12 : 8;
    case 0x38: return CPU_JUMP_IMMEDIATE(true, FLAG_C, true) ? 12 : 8;
    case 0x3F:
      m_RegisterAF.lo &= ~(1 << FLAG_N);
      m_RegisterAF.lo &= ~(1 << FLAG_H);
      m_RegisterAF.lo ^= (1 << FLAG_C);
      return 4;
    case 0xC1: CPU_16bit_PopToReg(m_RegisterBC); return 12;
    case 0xC2: return CPU_8bit_JP_2Byte_Imme(NZ) ? 16 : 12;
    case 0xC3: m_programCounter = ReadWord(); return 16;
    case 0xC4: return CPU_Call(false, FLAG_Z, true) ? 24 : 12;
    case 0xC5: PushWordToStack(m_RegisterBC.reg); return 16;
    case 0xC6: CPU_8bit_ADD(m_RegisterAF.hi, 0, true, false); return 8;
    case 0xC7: CPU_8bit_Restart(0x00); return 32;
    case 0xCE: CPU_8bit_ADD(m_RegisterAF.hi, 0, true, true); return 8;
    case 0xCC: return CPU_Call(true, FLAG_Z, true) ? 24 : 12;
    case 0xCD: CPU_Call(false, FLAG_C, false); return 24;
    case 0xCA: return CPU_8bit_JP_2Byte_Imme(Z) ? 16 : 12;

    case 0xCB: {
      byte cb_opcode = ReadMemory(m_programCounter);
      m_programCounter++;
      switch (cb_opcode) {
        case 0x00: CPU_8bit_RLC(m_RegisterBC.hi); return 8;
        case 0x01: CPU_8bit_RLC(m_RegisterBC.lo); return 8;
        case 0x02: CPU_8bit_RLC(m_RegisterDE.hi); return 8;
        case 0x03: CPU_8bit_RLC(m_RegisterDE.lo); return 8;
        case 0x04: CPU_8bit_RLC(m_RegisterHL.hi); return 8;
        case 0x05: CPU_8bit_RLC(m_RegisterHL.lo); return 8;
        case 0x06: {
          byte val = ReadMemory(m_RegisterHL.reg);
          CPU_8bit_RLC(val);
          WriteMemory(m_RegisterHL.reg, val);
          return 16;
        }
        case 0x07: CPU_8bit_RLC(m_RegisterAF.hi); return 8;
        case 0x08: CPU_8bit_RRC(m_RegisterBC.hi); return 8;
        case 0x09: CPU_8bit_RRC(m_RegisterBC.lo); return 8;
        case 0x0A: CPU_8bit_RRC(m_RegisterDE.hi); return 8;
        case 0x0B: CPU_8bit_RRC(m_RegisterDE.lo); return 8;
        case 0x0C: CPU_8bit_RRC(m_RegisterHL.hi); return 8;
        case 0x0D: CPU_8bit_RRC(m_RegisterHL.lo); return 8;
        case 0x0E: {
          byte val = ReadMemory(m_RegisterHL.reg);
          CPU_8bit_RRC(val);
          WriteMemory(m_RegisterHL.reg, val);
          return 16;
        }
        case 0x0F: CPU_8bit_RRC(m_RegisterAF.hi); return 8;
        case 0x10: CPU_8bit_RL(m_RegisterBC.hi); return 8;
        case 0x11: CPU_8bit_RL(m_RegisterBC.lo); return 8;
        case 0x12: CPU_8bit_RL(m_RegisterDE.hi); return 8;
        case 0x13: CPU_8bit_RL(m_RegisterDE.lo); return 8;
        case 0x14: CPU_8bit_RL(m_RegisterHL.hi); return 8;
        case 0x15: CPU_8bit_RL(m_RegisterHL.lo); return 8;
        case 0x16: {
          byte val = ReadMemory(m_RegisterHL.reg);
          CPU_8bit_RL(val);
          WriteMemory(m_RegisterHL.reg, val);
          return 16;
        }
        case 0x17: CPU_8bit_RL(m_RegisterAF.hi); return 8;
        case 0x18: CPU_8bit_RR(m_RegisterBC.hi); return 8;
        case 0x19: CPU_8bit_RR(m_RegisterBC.lo); return 8;
        case 0x1A: CPU_8bit_RR(m_RegisterDE.hi); return 8;
        case 0x1B: CPU_8bit_RR(m_RegisterDE.lo); return 8;
        case 0x1C: CPU_8bit_RR(m_RegisterHL.hi); return 8;
        case 0x1D: CPU_8bit_RR(m_RegisterHL.lo); return 8;
        case 0x1E: {
          byte val = ReadMemory(m_RegisterHL.reg);
          CPU_8bit_RR(val);
          WriteMemory(m_RegisterHL.reg, val);
          return 16;
        }
        case 0x1F: CPU_8bit_RR(m_RegisterAF.hi); return 8;
        case 0x20: CPU_8bit_SLA(m_RegisterBC.hi); return 8;
        case 0x21: CPU_8bit_SLA(m_RegisterBC.lo); return 8;
        case 0x22: CPU_8bit_SLA(m_RegisterDE.hi); return 8;
        case 0x23: CPU_8bit_SLA(m_RegisterDE.lo); return 8;
        case 0x24: CPU_8bit_SLA(m_RegisterHL.hi); return 8;
        case 0x25: CPU_8bit_SLA(m_RegisterHL.lo); return 8;
        case 0x26: {
          byte val = ReadMemory(m_RegisterHL.reg);
          CPU_8bit_SLA(val);
          WriteMemory(m_RegisterHL.reg, val);
          return 16;
        }
        case 0x27: CPU_8bit_SLA(m_RegisterAF.hi); return 8;
        case 0x28: CPU_8bit_SRA(m_RegisterBC.hi); return 8;
        case 0x29: CPU_8bit_SRA(m_RegisterBC.lo); return 8;
        case 0x2A: CPU_8bit_SRA(m_RegisterDE.hi); return 8;
        case 0x2B: CPU_8bit_SRA(m_RegisterDE.lo); return 8;
        case 0x2C: CPU_8bit_SRA(m_RegisterHL.hi); return 8;
        case 0x2D: CPU_8bit_SRA(m_RegisterHL.lo); return 8;
        case 0x2E: {
          byte val = ReadMemory(m_RegisterHL.reg);
          CPU_8bit_SRA(val);
          WriteMemory(m_RegisterHL.reg, val);
          return 16;
        }
        case 0x2F: CPU_8bit_SRA(m_RegisterAF.hi); return 8;
        case 0x30: CPU_8bit_SWAP(m_RegisterBC.hi); return 8;
        case 0x31: CPU_8bit_SWAP(m_RegisterBC.lo); return 8;
        case 0x32: CPU_8bit_SWAP(m_RegisterDE.hi); return 8;
        case 0x33: CPU_8bit_SWAP(m_RegisterDE.lo); return 8;
        case 0x34: CPU_8bit_SWAP(m_RegisterHL.hi); return 8;
        case 0x35: CPU_8bit_SWAP(m_RegisterHL.lo); return 8;
        case 0x36: {
          byte val = ReadMemory(m_RegisterHL.reg);
          CPU_8bit_SWAP(val);
          WriteMemory(m_RegisterHL.reg, val);
          return 16;
        }
        case 0x37: CPU_8bit_SWAP(m_RegisterAF.hi); return 8;
        case 0x38: CPU_8bit_SRL(m_RegisterBC.hi); return 8;
        case 0x39: CPU_8bit_SRL(m_RegisterBC.lo); return 8;
        case 0x3A: CPU_8bit_SRL(m_RegisterDE.hi); return 8;
        case 0x3B: CPU_8bit_SRL(m_RegisterDE.lo); return 8;
        case 0x3C: CPU_8bit_SRL(m_RegisterHL.hi); return 8;
        case 0x3D: CPU_8bit_SRL(m_RegisterHL.lo); return 8;
        case 0x3E: {
          byte val = ReadMemory(m_RegisterHL.reg);
          CPU_8bit_SRL(val);
          WriteMemory(m_RegisterHL.reg, val);
          return 16;
        }
        case 0x3F: CPU_8bit_SRL(m_RegisterAF.hi); return 8;
        default:
          if (cb_opcode >= 0x40 && cb_opcode <= 0x7F) {
            CPU_8bit_Bit_Test(cb_opcode);
            return ((cb_opcode & 7) == 6 ? 16 : 8);
          }
          if (cb_opcode >= 0xC0 && cb_opcode <= 0xFF) {
            CPU_8bit_BIT_SET(cb_opcode);
            return ((cb_opcode & 7) == 6 ? 16 : 8);
          }
          if (cb_opcode >= 0x80 && cb_opcode <= 0xBF) {
            CPU_8bit_BIT_RESET(cb_opcode);
            return ((cb_opcode & 7) == 6 ? 16 : 8);
          }
          return 0;
      }
    }
    case 0x37:
      m_RegisterAF.lo &= ~(1 << FLAG_N);
      m_RegisterAF.lo &= ~(1 << FLAG_H);
      m_RegisterAF.lo |= (1 << FLAG_C);
      return 4;
    case 0xC8: return CPU_RETURN(true, FLAG_Z, true) ? 20 : 8;
    case 0xC9: {
      word addr = PopWordFromStack();
      m_programCounter = addr;
      return 16;
    }
    case 0xC0: return CPU_RETURN(false, FLAG_Z, true) ? 20 : 8;
    case 0xD0: return CPU_RETURN(false, FLAG_C, true) ? 20 : 8;
    case 0xD1: CPU_16bit_PopToReg(m_RegisterDE); return 12;
    case 0xD2: return CPU_8bit_JP_2Byte_Imme(NC) ? 16 : 12;
    case 0xD4: return CPU_Call(false, FLAG_C, true) ? 24 : 12;
    case 0xD5: PushWordToStack(m_RegisterDE.reg); return 16;
    case 0xD6: CPU_8bit_SUB(m_RegisterAF.hi, 0, true, false); return 8;
    case 0xD8: return CPU_RETURN(true, FLAG_C, true) ? 20 : 8;
    case 0xD9: {
      word addr = PopWordFromStack();
      m_programCounter = addr;
      m_MasterInterrupt = true;
      m_EIpending = false;
      return 16;
    }

    case 0xDA: return CPU_8bit_JP_2Byte_Imme(C) ? 16 : 12;
    case 0xDC: return CPU_Call(true, FLAG_C, true) ? 24 : 12;
    case 0xDE: CPU_8bit_SUB(m_RegisterAF.hi, 0, true, true); return 8;
    case 0xE0: CPU_8bit_RegToImmeN0xFF00(m_RegisterAF.hi); return 12;
    case 0xE1: CPU_16bit_PopToReg(m_RegisterHL); return 12;
    case 0xE2: CPU_8bit_RegToC(m_RegisterAF.hi); return 8;
    case 0xE5: PushWordToStack(m_RegisterHL.reg); return 16;
    case 0xE6: CPU_8bit_AND(m_RegisterAF.hi, 0, true); return 8;
    case 0xE8: CPU_16bit_NToSP(); return 16;
    case 0xE9: m_programCounter = m_RegisterHL.reg; return 4;
    case 0xEA: CPU_8bit_RegToImmeMem(m_RegisterAF.hi); return 12;
    case 0xF0: CPU_8bit_ImmeN0xFF00ToReg(m_RegisterAF.hi); return 12;
    case 0xF1:
      CPU_16bit_PopToReg(m_RegisterAF);
      m_RegisterAF.lo &= 0xF0;
      return 12;
    case 0xF2: CPU_8bit_CToReg(m_RegisterAF.hi); return 8;
    case 0xF3:
      m_MasterInterrupt = false;
      m_EIpending = false;
      return 4;
    case 0xF5: PushWordToStack(m_RegisterAF.reg); return 16;
    case 0xF6: CPU_8bit_OR(m_RegisterAF.hi, 0, true); return 8;
    case 0xF8: CPU_16bit_SPNnToHL(); return 12;
    case 0xF9: CPU_16bit_Reg_Load(m_stackPointer, m_RegisterHL); return 8;
    case 0xFA: CPU_8bit_ImmeMemToReg(m_RegisterAF.hi); return 12;
    case 0xFB: m_EIpending = true; return 4;
    case 0xCF: CPU_8bit_Restart(0x08); return 32;
    case 0xD7: CPU_8bit_Restart(0x10); return 32;
    case 0xDF: CPU_8bit_Restart(0x18); return 32;
    case 0xE7: CPU_8bit_Restart(0x20); return 32;
    case 0xEF: CPU_8bit_Restart(0x28); return 32;
    case 0xF7: CPU_8bit_Restart(0x30); return 32;
    case 0xFF: CPU_8bit_Restart(0x38); return 32;
    default: return 4;
  }
}

void GameBoy::CPU_8bit_Load(byte& reg) {
  byte n = ReadMemory(m_programCounter);
  m_programCounter++;
  reg = n;
}

void GameBoy::CPU_8bit_Reg_Load(byte& reg1, byte& reg2) {
  reg1 = reg2;
}
void GameBoy::CPU_16bit_Reg_Load(Register& reg1, Register& reg2) {
  reg1 = reg2;
}

void GameBoy::CPU_8bit_ADD(byte& reg, byte toAdd, bool useImmediate,
                           bool addCarry) {
  byte before = reg;
  int adding = 0;

  if (useImmediate) {
    byte n = ReadMemory(m_programCounter);
    m_programCounter++;
    adding = n;
  } else {
    adding = toAdd;
  }
  int c = 0;
  if (addCarry) {
    if ((m_RegisterAF.lo & (1 << FLAG_C)) != 0) {
      c = 1;
    }
  }
  int result = before + adding + c;
  reg = (byte)result;
  m_RegisterAF.lo = 0;
  if (reg == 0) {
    m_RegisterAF.lo |= 1 << FLAG_Z;
  }
  int hCheck = (before & 0xF) + (adding & 0xF) + c;
  if (hCheck > 0xF) {
    m_RegisterAF.lo |= 1 << FLAG_H;
  }
  if (result > 0xFF) {
    m_RegisterAF.lo |= 1 << FLAG_C;
  }
}

void GameBoy::CPU_8bit_SUB(byte& reg, byte toSub, bool useImmediate,
                           bool borrowCarry) {
  byte before = reg;
  byte subbing = 0;

  if (useImmediate) {
    byte n = ReadMemory(m_programCounter);
    m_programCounter++;
    subbing = n;
  } else {
    subbing = toSub;
  }
  int c = 0;
  if (borrowCarry) {
    if ((m_RegisterAF.lo & (1 << FLAG_C)) != 0) {
      c = 1;
    }
  }
  int result = (int)before - (int)subbing - c;
  reg = (byte)result;
  m_RegisterAF.lo = 0;
  m_RegisterAF.lo |= (1 << FLAG_N);
  if (reg == 0) {
    m_RegisterAF.lo |= (1 << FLAG_Z);
  }
  if (((before & 0xF) - (subbing & 0xF) - c) < 0) {
    m_RegisterAF.lo |= (1 << FLAG_H);
  }
  if (result < 0) {
    m_RegisterAF.lo |= (1 << FLAG_C);
  }
}

void GameBoy::CPU_8bit_XOR(byte& reg, byte toXOR, bool useImmediate) {
  byte xoring = 0;
  if (useImmediate) {
    byte n = ReadMemory(m_programCounter);
    m_programCounter++;
    xoring = n;
  } else {
    xoring = toXOR;
  }
  reg = reg ^ xoring;
  m_RegisterAF.lo = 0;
  if (reg == 0) {
    m_RegisterAF.lo |= (1 << FLAG_Z);
  }
}

void GameBoy::CPU_8bit_AND(byte& reg, byte toAND, bool useImmediate) {
  byte anding = 0;
  if (useImmediate) {
    byte n = ReadMemory(m_programCounter);
    m_programCounter++;
    anding = n;
  } else {
    anding = toAND;
  }

  reg &= anding;
  m_RegisterAF.lo = 0;
  m_RegisterAF.lo |= (1 << FLAG_H);

  if (reg == 0) {
    m_RegisterAF.lo |= (1 << FLAG_Z);
  }
}

void GameBoy::CPU_8bit_OR(byte& reg, byte toOR, bool useImmediate) {
  byte oring = 0;
  if (useImmediate) {
    byte n = ReadMemory(m_programCounter);
    m_programCounter++;
    oring = n;
  } else {
    oring = toOR;
  }
  reg |= oring;
  m_RegisterAF.lo = 0;
  if (reg == 0) {
    m_RegisterAF.lo |= (1 << FLAG_Z);
  }
}

bool GameBoy::CPU_JUMP_IMMEDIATE(bool condition, int flag, bool useCondition) {
  signed char n = (signed_byte)ReadMemory(m_programCounter);
  if (!useCondition) {
    m_programCounter += n;
    m_programCounter++;
    return true;
  } else if ((((m_RegisterAF.lo & (1 << flag)) != 0) ? true : false) ==
             condition) {
    m_programCounter += n;
    m_programCounter++;
    return true;
  }
  m_programCounter++;
  return false;
}

bool GameBoy::CPU_Call(bool condition, int flag, bool useCondition) {
  word addr = ReadWord();
  m_programCounter += 2;
  if (!useCondition) {
    PushWordToStack(m_programCounter);
    m_programCounter = addr;
    return true;
  }
  if (((m_RegisterAF.lo & (1 << flag)) != 0 ? true : false) == condition) {
    PushWordToStack(m_programCounter);
    m_programCounter = addr;
    return true;
  }
  return false;
}

word GameBoy::ReadWord() {
  byte low = ReadMemory(m_programCounter);

  byte high = ReadMemory(m_programCounter + 1);
  return (high << 8) | low;
}

bool GameBoy::CPU_RETURN(bool condition, int flag, bool useCondition) {
  if (!useCondition) {
    m_programCounter = PopWordFromStack();
    return true;
  }

  if ((((m_RegisterAF.lo & (1 << flag)) != 0) ? true : false) == condition) {
    m_programCounter = PopWordFromStack();
    return true;
  }
  return false;
}

void GameBoy::CPU_8bit_MemToReg(byte& reg1, Register reg2, OP operation) {
  word addr = (reg2.hi << 8);
  addr |= reg2.lo;
  byte value = ReadMemory(addr);
  reg1 = value;
  switch (operation) {
    case NONE: break;
    case INC: m_RegisterHL.reg++; break;
    case DEC: m_RegisterHL.reg--;
  }
}

void GameBoy::CPU_8bit_RegToMem(Register reg1, byte reg2, OP operation) {
  word addr = (reg1.hi << 8) | reg1.lo;
  WriteMemory(addr, reg2);
  switch (operation) {
    case NONE: break;
    case INC: m_RegisterHL.reg++; break;
    case DEC: m_RegisterHL.reg--;
  }
}

void GameBoy::CPU_8bit_ImmeToMem(Register reg1) {
  byte n = ReadMemory(m_programCounter);
  word addr = (reg1.hi << 8) | reg1.lo;
  WriteMemory(addr, n);
  m_programCounter++;
}

void GameBoy::CPU_8bit_ImmeMemToReg(byte& reg1) {
  word addr = ReadWord();
  byte nn = ReadMemory(addr);
  reg1 = nn;
  m_programCounter += 2;
}

void GameBoy::CPU_8bit_RegToImmeMem(byte reg) {
  word addr = ReadWord();
  WriteMemory(addr, reg);
  m_programCounter += 2;
}

void GameBoy::CPU_8bit_RegToC(byte reg) {
  WriteMemory(0xFF00 | m_RegisterBC.lo, reg);
}

void GameBoy::CPU_8bit_RegToImmeN0xFF00(byte reg) {
  byte n = ReadMemory(m_programCounter);
  m_programCounter++;
  WriteMemory(0xFF00 + n, reg);
}

void GameBoy::CPU_8bit_ImmeN0xFF00ToReg(byte& reg) {
  byte n = ReadMemory(m_programCounter);
  m_programCounter++;
  byte data = ReadMemory(0xFF00 + n);
  reg = data;
}

void GameBoy::CPU_8bit_CToReg(byte& reg) {
  word data = ReadMemory(0xFF00 | m_RegisterBC.lo);
  reg = data;
}

void GameBoy::CPU_16bit_MemToReg(Register& reg) {
  word nn = ReadWord();
  m_programCounter += 2;
  reg.reg = nn;
}

void GameBoy::CPU_16bit_PopToReg(Register& reg) {
  word nn = PopWordFromStack();
  reg.reg = nn;
}

void GameBoy::CPU_16bit_SPNnToHL() {
  byte n = ReadMemory(m_programCounter);
  m_programCounter++;
  m_RegisterAF.lo = 0;
  signed_word signed_n = (signed_word)(signed_byte)n;
  m_RegisterHL.reg = m_stackPointer.reg + signed_n;
  if (((m_stackPointer.lo & 0xF) + (signed_n & 0xF) > 0xF)) {
    m_RegisterAF.lo |= (1 << FLAG_H);
  }
  if (((m_stackPointer.lo & 0xFF) + (signed_n & 0xFF) > 0xFF)) {
    m_RegisterAF.lo |= (1 << FLAG_C);
  }
}

void GameBoy::CPU_16bit_RegToImmeMem(Register reg) {
  word addr = ReadWord();
  m_programCounter += 2;
  WriteMemory(addr, reg.lo);
  WriteMemory(addr + 1, reg.hi);
}

void GameBoy::CPU_8bit_CP(byte reg, byte reg1) {
  m_RegisterAF.lo = 0;
  m_RegisterAF.lo |= (1 << FLAG_N);
  if (reg < reg1) {
    m_RegisterAF.lo |= (1 << FLAG_C);
  }
  if (reg == reg1) {
    m_RegisterAF.lo |= (1 << FLAG_Z);
  }
  if (((reg & 0xF) - (reg1 & 0xF)) < 0) {
    m_RegisterAF.lo |= (1 << FLAG_H);
  }
}
void GameBoy::CPU_8bit_INC(byte& reg, byte& flagReg) {
  reg++;
  flagReg &= (1 << FLAG_C);
  if (reg == 0) {
    flagReg |= (1 << FLAG_Z);
  }
  if ((reg & 0xF) == 0) {
    flagReg |= (1 << FLAG_H);
  }
}

void GameBoy::CPU_8bit_DEC(byte& reg, byte& flagReg) {
  reg--;
  flagReg &= (1 << FLAG_C);
  flagReg |= (1 << FLAG_N);
  if (reg == 0) {
    flagReg |= (1 << FLAG_Z);
  }
  if ((reg & 0xF) == 0xF) {
    flagReg |= (1 << FLAG_H);
  }
}
void GameBoy::CPU_8bit_SimOp(byte& reg, OP operation) {
  switch (operation) {
    case INC: CPU_8bit_INC(reg, m_RegisterAF.lo); break;
    case DEC: CPU_8bit_DEC(reg, m_RegisterAF.lo); break;
    default: break;
  }
}

void GameBoy::CPU_16bit_ADD(Register& reg, Register reg2, bool z_flag) {
  if (z_flag) {
    m_RegisterAF.lo &= (1 << FLAG_Z);
  } else {
    m_RegisterAF.lo = 0;
  }
  word result = reg.reg + reg2.reg;
  if (((reg.reg & 0xFFF) + (reg2.reg & 0xFFF)) > 0xFFF) {
    m_RegisterAF.lo |= (1 << FLAG_H);
  }
  if (((reg.reg & 0xFFFF) + (reg2.reg & 0xFFFF) > 0xFFFF)) {
    m_RegisterAF.lo |= (1 << FLAG_C);
  }
  reg.reg = result;
}

void GameBoy::CPU_16bit_NToSP() {
  byte n = ReadMemory(m_programCounter);
  m_programCounter++;
  byte lo = m_stackPointer.lo;
  signed_word signed_n = (signed_word)(signed_byte)n;
  m_stackPointer.reg = m_stackPointer.reg + signed_n;
  m_RegisterAF.lo = 0;
  if (((lo & 0xF) + (n & 0xF)) > 0xF) {
    m_RegisterAF.lo |= (1 << FLAG_H);
  }
  if ((lo + n) > 0xFF) {
    m_RegisterAF.lo |= (1 << FLAG_C);
  }
}

void GameBoy::CPU_8bit_SWAP(byte& reg) {
  reg = (reg << 4) | (reg >> 4);
  m_RegisterAF.lo = 0;
  if (reg == 0) {
    m_RegisterAF.lo |= (1 << FLAG_Z);
  }
}

void GameBoy::CPU_8bit_DAA() {
  byte& a = m_RegisterAF.hi;
  byte& f = m_RegisterAF.lo;

  if ((f & (1 << FLAG_N)) == 0) {
    if ((f & (1 << FLAG_C)) != 0 || a > 0x99) {
      a += 0x60;
      f |= (1 << FLAG_C);
    } else {
      f &= ~(1 << FLAG_C);
    }
    if ((f & (1 << FLAG_H)) != 0 || (a & 0x0F) > 0x09) {
      a += 0x06;
    }
  } else {
    if ((f & (1 << FLAG_C)) != 0) {
      a -= 0x60;
    }
    if ((f & (1 << FLAG_H)) != 0) {
      a -= 0x06;
    }
  }

  f &= ~(1 << FLAG_H);
  f &= ~(1 << FLAG_Z);
  if (a == 0) {
    f |= (1 << FLAG_Z);
  }
}

void GameBoy::CPU_8bit_RLC(byte& reg) {
  byte val = (reg >> 7) & 1;
  reg = (reg << 1) | val;
  m_RegisterAF.lo = 0;
  if (reg == 0) {
    m_RegisterAF.lo |= (1 << FLAG_Z);
  }
  m_RegisterAF.lo |= (val << FLAG_C);
}

void GameBoy::CPU_8bit_RL(byte& reg) {
  byte oldC = (m_RegisterAF.lo >> FLAG_C) & 1;
  byte old_bit7 = (reg >> 7) & 1;
  reg = (reg << 1) | oldC;
  m_RegisterAF.lo = 0;
  if (reg == 0) {
    m_RegisterAF.lo |= (1 << FLAG_Z);
  }
  if (old_bit7) {
    m_RegisterAF.lo |= (1 << FLAG_C);
  }
}

void GameBoy::CPU_8bit_RRC(byte& reg) {
  byte val = reg & 1;
  reg = (reg >> 1) | (val << 7);
  m_RegisterAF.lo = 0;
  if (reg == 0) {
    m_RegisterAF.lo |= (1 << FLAG_Z);
  }
  m_RegisterAF.lo |= (val << FLAG_C);
}

void GameBoy::CPU_8bit_RR(byte& reg) {
  byte oldC = (m_RegisterAF.lo >> FLAG_C) & 1;
  byte old_bit0 = reg & 1;
  reg = (reg >> 1) | (oldC << 7);
  m_RegisterAF.lo = 0;
  if (reg == 0) {
    m_RegisterAF.lo |= (1 << FLAG_Z);
  }
  if (old_bit0) {
    m_RegisterAF.lo |= (1 << FLAG_C);
  }
}

void GameBoy::CPU_8bit_SLA(byte& reg) {
  byte old_bit7 = (reg >> 7) & 1;
  reg = reg << 1;
  m_RegisterAF.lo = 0;
  if (reg == 0) {
    m_RegisterAF.lo |= (1 << FLAG_Z);
  }
  m_RegisterAF.lo |= (old_bit7 << FLAG_C);
}

void GameBoy::CPU_8bit_SRA(byte& reg) {
  byte old_bit7 = (reg >> 7) & 1;
  byte old_bit0 = reg & 1;
  reg = (reg >> 1) | (old_bit7 << 7);
  m_RegisterAF.lo = 0;
  if (reg == 0) {
    m_RegisterAF.lo |= (1 << FLAG_Z);
  }
  if (old_bit0) {
    m_RegisterAF.lo |= (1 << FLAG_C);
  }
}

void GameBoy::CPU_8bit_SRL(byte& reg) {
  byte old_bit0 = reg & 1;
  reg = reg >> 1;
  m_RegisterAF.lo = 0;
  if (reg == 0) {
    m_RegisterAF.lo |= (1 << FLAG_Z);
  }
  if (old_bit0) {
    m_RegisterAF.lo |= (1 << FLAG_C);
  }
}

void GameBoy::CPU_8bit_Bit_Test(byte opcode) {
  byte bit = (opcode >> 3) & 7;
  byte reg = opcode & 7;
  byte val = 0;
  switch (reg) {
    case 0: val = m_RegisterBC.hi; break;
    case 1: val = m_RegisterBC.lo; break;
    case 2: val = m_RegisterDE.hi; break;
    case 3: val = m_RegisterDE.lo; break;
    case 4: val = m_RegisterHL.hi; break;
    case 5: val = m_RegisterHL.lo; break;
    case 6: val = ReadMemory(m_RegisterHL.reg); break;
    case 7: val = m_RegisterAF.hi; break;
  }
  m_RegisterAF.lo &= (1 << FLAG_C);
  m_RegisterAF.lo |= (1 << FLAG_H);
  if (!(val & (1 << bit))) {
    m_RegisterAF.lo |= (1 << FLAG_Z);
  }
}

void GameBoy::CPU_8bit_BIT_SET(byte opcode) {
  byte bit = (opcode >> 3) & 7;
  byte reg = opcode & 7;
  switch (reg) {
    case 0: m_RegisterBC.hi |= (1 << bit); break;
    case 1: m_RegisterBC.lo |= (1 << bit); break;
    case 2: m_RegisterDE.hi |= (1 << bit); break;
    case 3: m_RegisterDE.lo |= (1 << bit); break;
    case 4: m_RegisterHL.hi |= (1 << bit); break;
    case 5: m_RegisterHL.lo |= (1 << bit); break;
    case 6: {
      byte val = ReadMemory(m_RegisterHL.reg);
      val |= (1 << bit);
      WriteMemory(m_RegisterHL.reg, val);
      break;
    }
    case 7: m_RegisterAF.hi |= (1 << bit); break;
  }
}

void GameBoy::CPU_8bit_BIT_RESET(byte opcode) {
  byte bit = (opcode >> 3) & 7;
  byte reg = opcode & 7;
  switch (reg) {
    case 0: m_RegisterBC.hi &= ~(1 << bit); break;
    case 1: m_RegisterBC.lo &= ~(1 << bit); break;
    case 2: m_RegisterDE.hi &= ~(1 << bit); break;
    case 3: m_RegisterDE.lo &= ~(1 << bit); break;
    case 4: m_RegisterHL.hi &= ~(1 << bit); break;
    case 5: m_RegisterHL.lo &= ~(1 << bit); break;
    case 6: {
      byte val = ReadMemory(m_RegisterHL.reg);
      val &= ~(1 << bit);
      WriteMemory(m_RegisterHL.reg, val);
      break;
    }
    case 7: m_RegisterAF.hi &= ~(1 << bit); break;
  }
}

bool GameBoy::CPU_8bit_JP_2Byte_Imme(CC cc) {
  word addr = ReadWord();
  switch (cc) {
    case 0:
      if ((m_RegisterAF.lo & (1 << FLAG_Z)) == 0) {
        m_programCounter = addr;
        return true;
      } else {
        m_programCounter += 2;
        return false;
      }
    case 1:
      if ((m_RegisterAF.lo & (1 << FLAG_Z)) != 0) {
        m_programCounter = addr;
        return true;
      } else {
        m_programCounter += 2;
        return false;
      }
    case 2:
      if ((m_RegisterAF.lo & (1 << FLAG_C)) == 0) {
        m_programCounter = addr;
        return true;
      } else {
        m_programCounter += 2;
        return false;
      }
    case 3:
      if ((m_RegisterAF.lo & (1 << FLAG_C)) != 0) {
        m_programCounter = addr;
        return true;
      } else {
        m_programCounter += 2;
        return false;
      }
    default: m_programCounter += 2; return false;
  }
}

void GameBoy::CPU_8bit_Restart(byte addr) {
  PushWordToStack(m_programCounter);
  m_programCounter = addr;
}
