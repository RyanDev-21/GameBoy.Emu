#include "GameBoy.hpp"

// 0xFF00 register store the state of the joyPad
// Bit-7 unused
// Bit-6 unused
// Bit-5 P15 Select button state(0=select,1=not select)
// Bit-4 P14 Select direction state(0=select,1=not select)
// Bit 3 - P13 Input Down or Start(0 = Pressed)(Read Only)
// Bit 2 - P12 Input Up or Select(0 = Pressed)(Read Only)
// Bit 1 - P11 Input Left or Button B(0 = Pressed)(Read Only)
// Bit 0 - P10 Input Right or Button A(0 = Pressed)(Read Only)
// m_JoyPadState has a byte
// 0-bit = Right
// 1-bit = Left
// 2-bit = Up
// 3-bit = Down
// 4-bit = A
// 5-bit = B
// 6-bit = Select
// 7-bit = Start
// 0xFF00 is read only and it only returns the snapshot not detail
// for example
// if two group(button,direction) pressed
// 1100 1100 should be the state it returns
byte GameBoy::GetJoyPadState() const {
  byte joyP = m_rom[0xFF00];  // to know the group select
  // we only care about the higher nibles
  joyP |= 0x0F;  // turn the lower 4 bit all 1
  // button case
  if ((joyP & 0x20) == 0) {  // test the 5th bit
    // shift the upper bit to lower bit of joyPadState
    // as the joyPadState store the button bit on higher ones
    byte topjoyPad = (m_joyPadState >> 4) & 0x0F;
    joyP &= (0xF0 | topjoyPad);
  }
  // direction case
  if ((joyP & 0x10) == 0) {  // test the 4th bit
    // doesn't need shift as direciton are already in lower bit
    byte lowJoyPad = m_joyPadState & 0xF;  // turn of the higher bit if any set
    joyP &= (lowJoyPad | 0xF0);
  }
  return joyP;
}

void GameBoy::KeyPressed(int key) {
  bool previouslyUnset = false;
  // check if the current key is already 0
  if ((m_joyPadState & (1 << key)) == 0) {
    previouslyUnset = true;
  }
  m_joyPadState &= ~(1 << key);  // force to 0
  bool button = false;
  // as direction are below 3
  if (key > 3) {
    button = true;
  }
  byte keyReq = m_rom[0xFF00];
  bool requestInterrupt = false;

  if (button && ((keyReq & (1 << 5)) == 0)) {
    requestInterrupt = true;
  } else if (!button && ((keyReq & (1 << 4)) == 0)) {
    requestInterrupt = true;
  }

  // if not the same unset
  if (requestInterrupt && !previouslyUnset) {
    RequestInterrupt(4);
  }
}

// just force that bit to 1
void GameBoy::KeyReleased(int key) {
  m_joyPadState |= (1 << key);
}
