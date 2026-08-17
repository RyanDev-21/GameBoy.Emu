#include <cmath>
#include <cstdint>
#include <cstring>

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
    m_Div++;                  // increase the divider register
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

void GameBoy ::updateRTC(float secondsElapsed) {
  if ((m_RTCregs[4] & 0x40) != 0) {
    return;
  }
  m_RTCaccumulator += secondsElapsed;
  while (m_RTCaccumulator >= 1.0) {
    m_RTCaccumulator -= 1.0;
    tickOneSecond();
  }
};

void GameBoy::tickOneSecond() {
  m_RTCregs[0]++;
  if (m_RTCregs[0] <= 59)
    return;
  m_RTCregs[0] = 0;

  // minute
  m_RTCregs[1]++;
  if (m_RTCregs[1] <= 59) {
    return;
  }
  m_RTCregs[1] = 0;

  // hour
  m_RTCregs[2]++;
  if (m_RTCregs[2] <= 23) {
    return;
  }
  m_RTCregs[2] = 0;

  // day
  m_RTCregs[3]++;
  if (m_RTCregs[3] <= 0xFF) {
    return;
  }
  m_RTCregs[3] = 0;

  char dayMSB = (m_RTCregs[4] & 0x01) ^ 0x01;
  m_RTCregs[4] = (m_RTCregs[4] & 0xFE) | dayMSB;

  if (dayMSB == 0) {
    m_RTCregs[4] |= 0x80;
  }
};

void GameBoy::fastForwardRTC(float secondsElapsed) {
  if ((m_RTCregs[4] & 0x40) != 0) {
    return;
  }
  int days = m_RTCregs[3] | (m_RTCregs[4] & 0x01);
  int64_t total_seconds =
      m_RTCregs[0] + m_RTCregs[1] * 60 + m_RTCregs[2] * 3600 + days * 86400;
  total_seconds += static_cast<int64_t>(std::floor(secondsElapsed));
  const bool overflow = total_seconds >= 512 * 86400;
  total_seconds %= 512 * 86400;

  const bool previousOverflow = (m_RTCregs[4] & 0x80) != 0;

  m_RTCregs[0] = total_seconds % 60;
  m_RTCregs[1] = std::floor((total_seconds / 60) % 60);
  m_RTCregs[2] = std::floor((total_seconds / 3600) % 24);

  const uint16_t newDays = std::floor(total_seconds / 86400);
  m_RTCregs[3] = newDays & 0xFF;

  byte dh = (newDays >> 8) & 0x01;
  if (overflow) {
    dh |= 0x80;
  }
  if ((m_RTCregs[4] & 0x40) != 0) {
    dh |= 0x40;
  }
  m_RTCregs[4] = dh;
  memcpy(m_RTCLatch, m_RTCregs, sizeof(m_RTCregs));
};
