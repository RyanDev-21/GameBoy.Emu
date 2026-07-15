#include "GameBoy.hpp"

// Timer address
#define TIMA 0xFF05
#define TMA 0xFF06
#define TMC 0xFF07

void GameBoy::UpdateTimers(int cycles) {
  // update the divider register
  DoDividerCounter(cycles);
  // only if the timer clock is enabled
  if (TimerClockEnabled()) {
    m_TimerCounter -= cycles;
    if (m_TimerCounter <= 0) {        // when finished
      SetClockFeq();                  // reset timer counter
      if (ReadMemory(TIMA) == 255) {  // time about to overflow
        // TMA value stores the starting count for interrupt
        WriteMemory(TIMA, ReadMemory(TMA));  // reset with TMA value
        RequestInterrupt(2);
      } else {
        WriteMemory(TIMA, ReadMemory(TIMA) + 1);  // plus the current timer
      }
    }
  }
}

// track the divider reigster
void GameBoy::DoDividerCounter(int cycles) {
  m_DividerCounter += cycles;
  if (m_DividerCounter >= 256) {
    m_DividerCounter -= 256;  // reset
    m_rom[0xFF04]++;          // increase the divider register
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
byte GameBoy::GetClockFeq() const {
  return ReadMemory(TMC) & 3;
}

// ClockSpeed of GameBoy is 4,194,304
// timerCounter = ClockSpeed/feq
void GameBoy::SetClockFeq() {
  byte feq = GetClockFeq();

  switch (feq) {
    case 0:                   // 00
      m_TimerCounter = 1024;  // feq 4096
      break;
    case 1:                 // 01
      m_TimerCounter = 16;  // feq 262,144
      break;
    case 2:                 // 10
      m_TimerCounter = 64;  // feq 65536
      break;
    case 3:                  // 11
      m_TimerCounter = 256;  // feq 16,382
      break;
  }
}
