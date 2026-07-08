#include "./GameBoy.hpp"
#include <cstdio>
#include <cstring>

// Timer address
#define TIMA 0xFF05
#define TMA 0xFF06
#define TMC 0xFF07

// The consturctor will initialize and then set the required state of the
// emulator as if the real game has started gameboy doesn't have an isolate
// stack that's why it is allocated at the near end of higher byte after the
// interept slot 0xFFFF
GameBoy::GameBoy()
    :

      m_programCounter(0x100), m_RegisterAF{.reg = 0x01B0},
      m_RegisterBC{.reg = 0x0013}, m_RegisterDE{.reg = 0x00D8},
      m_RegisterHL{.reg = 0x014D}, current_romBank(1), current_ramBank(0),
      m_MBU1(false), m_MBU2(false), m_enableRAM(false), m_EIpending(0),
      m_Halt(false) {
  // allocate stack pointer
  m_stackPointer.reg = 0xFFFE;
  // init the ramBanks
  memset(&m_ramBanks, 0, sizeof(m_ramBanks));
  // Memmory Controller
  // set the ram bank
  switch (m_CartridgeMemory[0x147]) {
  case 1:
    m_MBU1 = true;
    break;
  case 2:
    m_MBU1 = true;
    break;
  case 3:
    m_MBU1 = true;
    break;
  case 5:
    m_MBU2 = true;
    break;
  case 6:
    m_MBU2 = true;
    break;

  default:
    break;
  }

  m_rom[0xFF05] = 0x00; // TIMA
  m_rom[0xFF06] = 0x00; // TMA
  m_rom[0xFF07] = 0x00; // TAC
  m_rom[0xFF10] = 0x80; // NR10
  m_rom[0xFF11] = 0xBF; // NR11
  m_rom[0xFF12] = 0xF3; // NR12
  m_rom[0xFF14] = 0xBF; // NR14
  m_rom[0xFF16] = 0x3F; // NR21
  m_rom[0xFF17] = 0x00; // NR22
  m_rom[0xFF19] = 0xBF; // NE24
  m_rom[0xFF1A] = 0x7F; // NR30
  m_rom[0xFF1B] = 0xFF; // NR31
  m_rom[0xFF1C] = 0x9F; // NR32
  m_rom[0xFF1E] = 0xBF; // NR33
  m_rom[0xFF20] = 0xFF; // NR41
  m_rom[0xFF21] = 0x00; // NR42
  m_rom[0xFF22] = 0x00; // NR43
  m_rom[0xFF23] = 0xBF; // NR30
  m_rom[0xFF24] = 0x77; // NR50
  m_rom[0xFF25] = 0xF3; // NR51
  m_rom[0xFF26] = 0xF1; // NR52
  m_rom[0xFF42] = 0x00; // LCDC
  m_rom[0xFF43] = 0x00; // SCY
  m_rom[0xFF40] = 0x91; // SCX
  m_rom[0xFF45] = 0x00; // LYC
  m_rom[0xFF47] = 0xFC; // BGP
  m_rom[0xFF48] = 0xFF; // OBP0
  m_rom[0xFF49] = 0xFF; // OBP1
  m_rom[0xFF4A] = 0x00; // WY
  m_rom[0xFF4B] = 0x00; // WX
  m_rom[0xFFFF] = 0x00; // IE
}

void GameBoy::WriteMemory(word address, byte data) {
  // if within the switching range then handle the switching
  if (address < 0x8000) {
    HandleBanking(address, data);
  }
  // if writing to the dividerRegister
  else if (address == 0xFF04) {
    m_rom[0xFF04] = 0; // reset to 0
  }

  // if writing to the scanline
  else if (address == 0xFF44) {
    m_rom[0xFF44] = 0; // reset
  }
  // if writing to the dma source address
  else if (address == 0xFF46) {
    DoDMATransfer(data);
  }

  // if writing to TMC
  else if (address == TMC) {
    byte currentFeq = GetClockFeq();
    m_rom[TMC] = data; // this can change the enable bit or feq bits
    byte newFeq = GetClockFeq();
    if (currentFeq != newFeq) { //  change in feq
      SetClockFeq();            // set new one
    }
  }

  // if within the switchable ram range
  else if (address >= 0xA000 && address <= 0xC000) {
    if (m_enableRAM) {
      word new_addr = address - 0xA000;
      m_CartridgeMemory[new_addr + (current_ramBank * 0x2000)] = data;
    }
  }

  // writing in echo region
  else if ((address >= 0xE000) && (address < 0xFE00)) {
    m_rom[address] = data;
    WriteMemory(address - 0x2000, data); // has to write into wram space too
  }
  // no write allowed for these regions
  else if (address >= 0xFEA0 && address < 0xFEFF) {
  }
  // others
  else {
    m_rom[address] = data;
  }
};

void GameBoy::ReadRom(char const *filePath) {
  memset(&m_CartridgeMemory, 0, sizeof(m_CartridgeMemory));
  FILE *in;

  in = fopen(filePath, "rb");
  fread(m_CartridgeMemory, 1, 0x200000, in);
  fclose(in);
};

byte GameBoy::ReadMemory(word address) const {
  // are we reading from switchable rom
  if (address >= 0x4000 && address <= 0x7FFF) {
    // shift the offset to start;
    word new_addr = address - 0x4000;
    // each rom bank is 0x4000 so have to multiply to offset it
    return m_CartridgeMemory[new_addr + (current_romBank * 0x4000)];
  }

  // are we reading from switchable ram
  else if (address >= 0xA000 && address <= 0xBFFF) {
    // shif the offset
    word new_addr = address - 0xA000;
    // each ram bank is 0x2000
    return m_ramBanks[new_addr + (current_ramBank * 0x2000)];
  }
  // others region? return
  else {
    return m_rom[address];
  }
};

void GameBoy::HandleBanking(word address, byte data) {
  // do RAM enabling
  if (address < 0x2000) {
    if (m_MBU1 || m_MBU2) {
      DoRAMBanking(address, data);
    }
  }
  // do RAM switching
  else if (address >= 0x2000 && address <= 0x3FFF) {
    if (m_MBU1 || m_MBU2) {
      DoChangeLoROMBank(data);
    }
  }
  // do RAM or ROM switching
  else if (address >= 0x4000 && address < 0x6000) {
    // no ramBank in Mbu2 alwasy uses ramBank 0
    if (m_MBU1) {
      if (m_enableROM) {
        DoChangeHiROMBank(data);
      } else {
        DoChangeRAMBank(data);
      }
    }
  }
  // do ROM enable
  else if (address >= 0x6000 && address < 0x8000) {
    if (m_MBU1) {
      DoChangeROMRAMBank(data);
    }
  }
}

void GameBoy::DoRAMBanking(word address, byte data) {
  if (m_MBU2) {
    if ((address & 0x10) == 1) {
      return;
    }
  }
  byte new_data = data & 0xF;
  if (new_data == 0xA) {
    m_enableRAM = true;
  } else if (new_data == 0x0) {
    m_enableRAM = false;
  }
}

void GameBoy::DoChangeLoROMBank(byte data) {
  if (m_MBU2) {
    current_romBank = data & 0xF;
    if (current_romBank == 0) {
      current_romBank++;
    }
    return;
  }
  // 31 in binary =0001 1111
  byte low5 = data & 31; // lower 5 bits
  // 224 in binary = 1110 0000
  current_romBank &= 224;  // turn of lower 5 bit
  current_romBank |= low5; // merge
  if (current_romBank == 0) {
    current_romBank++;
  }
}

void GameBoy::DoChangeHiROMBank(byte data) {
  current_romBank &= 31;       // take out 5 bit
  byte new_data = data & 224;  // take out 3 bit
  current_romBank |= new_data; // merge
  if (current_romBank == 0) {
    current_romBank++;
  }
}

// Only take out the lower 2 bit
// Max Bank count support in MBU1 = 4
// 00 =0
// 01 =1
// 10 =2
// 11 =3
void GameBoy::DoChangeRAMBank(byte data) { current_ramBank = data & 3; }

void GameBoy::DoChangeROMRAMBank(byte data) {
  byte new_data = data & 0x1; // take out 1 bit
  m_enableROM = (new_data == 0) ? true : false;
  // reset  ramBank to 0
  if (m_enableROM) {
    current_ramBank = 0;
  }
}

void GameBoy::UpdateTimers(int cycles) {
  // update the divider register
  DoDividerCounter(cycles);
  // only if the timer clock is enabled
  if (TimerClockEnabled()) {
    m_TimerCounter -= cycles;
    if (m_TimerCounter <= 0) {       // when finished
      SetClockFeq();                 // reset timer counter
      if (ReadMemory(TIMA) == 255) { // time about to overflow
        // TMA value stores the starting count for interrupt
        WriteMemory(TIMA, ReadMemory(TMA)); // reset with TMA value
        RequestInterrupt(2);
      } else {
        WriteMemory(TIMA, ReadMemory(TIMA) + 1); // plus the current timer
      }
    }
  }
};

// track the divider reigster
void GameBoy::DoDividerCounter(int cycles) {
  m_DividerCounter += cycles;
  if (m_DividerCounter >= 255) {
    m_DividerCounter = 0; // reset
    m_rom[0xFF04]++;      // increase the divider register
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
byte GameBoy::GetClockFeq() const { return ReadMemory(TMC) & 3; }

// ClockSpeed of GameBoy is 4,194,304
// timerCounter = ClockSpeed/feq
void GameBoy::SetClockFeq() {
  byte feq = GetClockFeq();

  switch (feq) {
  case 0:                  // 00
    m_TimerCounter = 1024; // feq 4096
    break;
  case 1:                // 01
    m_TimerCounter = 16; // feq 262,144
    break;
  case 2:                // 10
    m_TimerCounter = 64; // feq 65536
    break;
  case 3:                 // 11
    m_TimerCounter = 256; // feq 16,382
    break;
  }
}

void GameBoy::RequestInterrupt(int id) {
  byte req = ReadMemory(0xFF0F);
  req |= (1 << id);         // merge what ever id position bit
  WriteMemory(0xFF0F, req); // update the req
}

// Interrupt Bit priority
// Bit 0: V-Blank
// Bit 1:LCD
// Bit 2:Timer
// Bit 3:Joypad
void GameBoy::DoInterrupts() {
  if (m_MasterInterrupt) {
    byte req = ReadMemory(0xFF0F);
    byte enabled = ReadMemory(0xFFFF);
    if (req > 0) {
      for (int i = 0; i < 5; i++) {
        if ((req & (1 << i)) == 1) {
          if ((enabled & (1 << i)) == 1) {
            ServiceInterrupt(i);
          }
        }
      }
    }
  }
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
  req ^= (1 << interrupt); // flip the bit
  WriteMemory(0xFF0F, req);

  PushWordToStack(m_programCounter);
  switch (interrupt) {
  case 0:
    m_programCounter = 0x40;
    break;
  case 1:
    m_programCounter = 0x48;
    break;
  case 2:
    m_programCounter = 0x50;
    break;
  case 3:
    m_programCounter = 0x60;
    break;
  }
}

// Store the data to stack and update the stack pointer
void GameBoy::PushWordToStack(word data) {
  byte high = (data >> 8) & 0xFF;
  byte low = data & 0xFF;

  // gameboy can only store 8bit so
  // has to store high an low
  m_stackPointer.reg--;
  WriteMemory(m_stackPointer.hi, high);
  m_stackPointer.reg--;
  WriteMemory(m_stackPointer.lo, low);
}

word GameBoy::PopWordFromStack() {
  byte high = ReadMemory(m_stackPointer.reg);
  m_stackPointer.reg++;
  byte low = ReadMemory(m_stackPointer.reg);
  m_stackPointer.reg++;
  return (high << 8) | low;
}

// Updae the scanline lcd
// The GameBoy has 160x144
// Scanline start from 0 to 153
// 144 visible scanline && 10 invisible scanline
// Takes 456 cycles for 1 scanline to finish
void GameBoy::UpdateGraphics(int cycles) {
  SetLCD_status();
  if (LCD_enabled()) {
    m_scalineCounter -= cycles;
  } else {
    return;
  }
  if (m_scalineCounter <= 0) { // time to move to the next line
    m_rom[0xFF44]++;
    byte currentLine = ReadMemory(0xFF44);
    m_scalineCounter = 456;   // reset the cycle count
    if (currentLine == 144) { // about to enter v-blank
      RequestInterrupt(0);
    } else if (currentLine > 153) { // reset to the top
      m_rom[0xFF44] = 0;
    } else if (currentLine < 144) {
      // DrawScanLine
    }
  }
}

// Check lcd_enabled
bool GameBoy::LCD_enabled() {
  return ((ReadMemory(0xFF40) & 128) == 0) ? false : true;
}

// LCD mode are stored in 0 and 1 bit of  0xFF41
// ....LCD modes.....
// 00: H-blank
// 01: V-Blank
// 10: Search for sprite //start from here and loops to the V-Blank
// 11: Transfer data to LCD
// The other bits represent Interrupt
// Bit 2: Flip to 1 when 0xFF44 and 0xFF45 are the same(coincidence flag)
// Bit 3: Mode 0 Interupt Enabled
// Bit 4: Mode 1 Interupt Enabled
// Bit 5: Mode 2 Interupt Enabled
// Bit 6: Interrupt for Bit 2
// LCD interrupt will tirgger when mode and their interrupt bit  are set
//  Set LCD Status
void GameBoy::SetLCD_status() {
  byte status = ReadMemory(0xFF41);
  if (LCD_enabled() == false) {
    m_scalineCounter = 456;
    m_rom[0xFF44] = 0; // scaline has to set to  0
    status &= 252;     // make the first two bit to 0
    status |= 1;       // turn on the first bit
    WriteMemory(0xFF41, status);
    return;
  }
  byte currentLine = ReadMemory(0xFF44);
  byte currentMode = status & 0x3;
  byte mode = 0;
  bool reqInt = false;

  // v-blank situation
  if (currentLine >= 144) {
    mode = 1;                    // v-blank
    status &= 252;               // make 0 to first two bit
    status |= 1;                 // turn on first bit
    reqInt = (status & 16) != 0; // check interrupt
  } else {
    int mode2Counts = 456 - 82;          // mode 2 use  82cycles
    int mode3Counts = mode2Counts - 172; // mode 3 use 172 cycles

    // search sprite situation
    if (m_scalineCounter >= mode2Counts) {
      mode = 2;
      status &= 252;
      status |= 2;
      reqInt = (status & 32) != 0;
    }

    // transfer data to lcd driver situation
    if (m_scalineCounter >= mode3Counts) {
      mode = 3;
      status &= 252;
      status |= 3;
      // mode3 doesn't have interrupt

    } else { // h-blank situation
      mode = 0;
      status &= 252;
      reqInt = (status & 8) != 0;
    }
  }
  if (reqInt && (mode != currentMode)) { //  mode switch
    RequestInterrupt(1);                 // LCD interrupt
  }
  if (currentLine == ReadMemory(0xFF45)) { // coincidence
    status |= 4;                           // turn on the 2th bit
    if ((status & 64) != 0) {              // if interrupt enabled
      RequestInterrupt(1);
    }
  } else {
    status &= ~4; // turn off  bit 2
  }
  WriteMemory(0xFF41, status);
}

// GameBoy doesn't allow the sprite ram(OAM) to update and delete when it is
// drawing. Only allow during v-blank, so to update during that v-blank period
// it is not enough time to update whole in that scenario, devs use dma DMA
// destination address(0xFE00-0xFE9F) which is exacly 0xA0 byte
void GameBoy::DoDMATransfer(byte address) {
  // 0xFF46 has the source address and only store 8 bit
  word targetAddr = address << 8; // shift by right 8 to form 16bit
  for (int i = 0; i < 0xA0; i++) {
    WriteMemory(0xFE00 + i, ReadMemory(targetAddr + i));
  }
}
// the control register:0xFF40
// bit 7=LCD display(on=1,off=0)
// bit 6=Window tile map select(0=9800-9BFF,1=9C00-9FFF)
// bit 5 - Window Display Enable (0=Off, 1=On)
// bit 4 - BG & Window Tile Data Select (0=8800-97FF, 1=8000-8FFF)
// bit 3 - BG Tile Map Display Select (0=9800-9BFF, 1=9C00-9FFF)
// bit 2 - OBJ (Sprite) Size (0=8x8, 1=8x16)
// bit 1 - OBJ (Sprite) Display Enable (0=Off, 1=On)
// bit 0 - BG Display  (0=Off, 1=On)
void GameBoy::DrawScanLine() {
  byte status = ReadMemory(0xFF40);
  if ((status & 1) != 0) { // check bit 0
    RenderTiles();
  }

  if ((status & 2) != 0) { // check bit 1
    RenderSprites();
  }
}

// ScrollX and Y are background position
// WindowX and y are window view position
// WindowX has to subtract 7 in order to make the prefetch for ppu and prevent
// empty data to draw but here, we are not doing that fifo implementation for
// ppu and we just draw whatever has at 0
void GameBoy::RenderTiles() {
  word backgroundMem = 0;
  word tileData = 0;
  byte status = ReadMemory(0xFF40);
  bool unsig = false;
  bool usingWindow = false;
  byte scrollY = ReadMemory(0xFF42);
  byte scrollX = ReadMemory(0xFF43);
  byte windowY = ReadMemory(0xFF4A);
  byte windowX =
      ReadMemory(0xFF4B) - 7; // in order for the ppu to prefetch the data

  // check the window bit
  if ((status & 32) != 0) { // check bit 5
    if (windowY <=
        ReadMemory(0xFF44)) { // only if y is less than or equal to scanline
      usingWindow = true;
    }
  }
  // check the tile data select bit
  if ((status & 16) != 0) { // check bit 4
    tileData = 0x8000;
    unsig = true;
  } else {
    tileData = 0x8800; // this memory region use signed byte
  }

  // check the window & background select bit
  if (false == usingWindow) {
    // which background memory region
    if ((status & 8) != 0) { // check bit 3
      backgroundMem = 0x9C00;
    } else {
      backgroundMem = 0x9800;
    }
  } else {
    // whcih window memory region
    if ((status & 64) != 0) { // check bit 6
      backgroundMem = 0x9C00;
    } else {
      backgroundMem = 0x9800;
    }
  }

  byte yPos = 0;

  if (!usingWindow) {
    // just plus the background position with scanline
    yPos = scrollY + ReadMemory(0xFF44);
  } else {
    // we have to reset the yPos  as we need to get the correct index for tile
    // data
    yPos = ReadMemory(0xFF44) - windowY;
  }
  // each tile is 8x8
  // so to get the tileRow index(current scanline pixel) we have to divide by  8
  // to yPos and multiply 32 to jump to corret index
  word tileRow = (((byte)(yPos / 8)) * 32); // vertical tiles index
  // draw the horizontal pixel
  for (int pixel = 0; pixel < 160; pixel++) {
    byte xPos = scrollX + pixel;
    if (usingWindow) {
      if (pixel >= windowX) {   // at drawing window pixel
        xPos = pixel - windowX; // reset
      }
    }
    // horizontal tile index
    word tileCol = xPos / 8;
    signed_word tileNum;
    // plus all three index and get the actual tileAddress
    word tileAddress = backgroundMem + tileRow + tileCol;
    if (unsig) {
      //  unsig
      //  as the signed_word is 16 bit it can handle the unsigned_byte(8 bit)
      tileNum = (byte)ReadMemory(tileAddress);
    } else {
      // sig
      tileNum = (signed_word)ReadMemory(tileAddress);
    }

    word tileLocation = tileData;
    if (unsig) {
      tileLocation += (tileNum * 16);
    } else {
      // make the signed_word positive
      // for example;
      // signed_byte : -128 to 127
      //  -128 means the starting address 0;
      // so we get the actual address from 0x8800
      tileLocation += (tileNum + 128) * 16;
    }
    // find the tile index of the current scanline to get the tileData
    byte index = yPos % 8;
    index *= 2; // as two byte are taken for color bit in memory
    byte data1 = ReadMemory(tileLocation + index);
    byte data2 = ReadMemory(tileLocation + index + 1);

    // pixel 0 correspond to data1 & data2 's bit 7
    // pixel 1 bit 6 ...
    byte colorBit = xPos % 8;
    colorBit -= 7;
    colorBit *= -1;
    int colorNum = ((data2 >> colorBit) & 1) << 1;
    colorNum |= (data1 << colorBit) & 1;
    // get actual color  based on the colorNum and colorPalette
    byte color = ReadColor(colorNum, 0xFF47);
    int red = 0;
    int green = 0;
    int blue = 0;
    switch (color) {
    case WHITE:
      red = 255;
      green = 255;
      blue = 255;
      break;
    case LIGHT_GRAY:
      red = 0xCC;
      green = 0xCC;
      blue = 0xCC;
      break;
    case DARK_GRAY:
      red = 0x77;
      green = 0x77;
      blue = 0x77;
      break;
    }
    // read the current pixel or lcd scanline coord
    byte finally = ReadMemory(0xFF44);
    // determine whether fall within the display region
    if ((finally < 0 || finally > 143) ||
        (pixel < 0 || pixel > 159)) { // if not fall within display region
      continue;
    }

    m_screenData[pixel][finally][0] = red;
    m_screenData[pixel][finally][1] = green;
    m_screenData[pixel][finally][2] = blue;
  }
}

// Color Code Map to Color Palette
// 00:White      bit 1-0
// 01:Light Grey bit 3-2
// 10:Dark Grey  bit 5-4
// 11:Black bit 7-6
COLOUR GameBoy::ReadColor(int colorNum, word address) {
  COLOUR res = WHITE;
  int hi = 0;
  int lo = 0;
  byte colorPalette = ReadMemory(address);
  switch (colorNum) {
  case 0:
    hi = 1;
    lo = 0;
    break;
  case 1:
    hi = 3;
    lo = 2;
    break;
  case 2:
    hi = 5;
    lo = 4;
    break;
  case 3:
    hi = 7;
    lo = 6;
    break;
  }

  int colour = 0;
  // lookup the colorbit in the color palette
  // shift to right first to do mask
  // shift to left to make the bit correct
  colour = ((colorPalette >> hi) & 1) << 1;
  // merge with low bit
  colour |= (colorPalette >> lo) & 1;

  // return enum based on the color type
  switch (colour) {
  case 0:
    res = WHITE;
    break;
  case 1:
    res = LIGHT_GRAY;
    break;
  case 2:
    res = DARK_GRAY;
    break;
  case 3:
    res = BLACK;
    break;
  }

  return res;
}

// Spirte RAM region:0x8000-0x8FFF
// Sprite Attri region:0xFE00-0xFE9F
void GameBoy::RenderSprites() {
  bool use8x16 = ((ReadMemory(0xFF40) & 4) != 0)
                     ? true
                     : false; // this bit 4 in control lcd register tells
                              // whether the sprite is 8x8 or 8x16
  bool flipY = false;
  bool flipX = false;
  // each frame can render 40 sprite
  for (int sprite = 0; sprite < 40; sprite++) {
    // each sprite takes 4 bytes
    byte index = sprite * 4;
    // each of this is 1 byte = total=4
    byte yPos = ReadMemory(0xFE00 + index) - 16;
    byte xPos = ReadMemory(0xFE00 + index + 1) - 8;
    byte tileLocation =
        ReadMemory(0xFE00 + index + 2); // this one is for sprite pattern number
    byte attributes = ReadMemory(0xFE00 + index +
                                 3); // this attri tells about the sprite
                                     // Attribute bit
                                     // bit 7 priority
                                     // bit 6 y flip
                                     // bit 5 x flip
                                     // bit 4 palette number (0=0xFF48,1=0xFF49)
    flipY = ((attributes & 64) != 0) ? true : false;
    flipX = ((attributes & 32) != 0) ? true : false;
    byte scanline = ReadMemory(0xFF44);
    int ySize = 8;
    if (use8x16) {
      ySize = 16;
    }
    if ((scanline >= yPos) &&
        (scanline <= yPos + ySize)) { // within the y draw range
      byte line = scanline - yPos;
      if (flipY) {
        // covert the 0 to last if the line is 0
        // so fliping is just reading from last rather than from start
        line -= ySize;
        line *= -1;
      }
      line *= 2; // same as tiles
      // each row in tile takes exacly two bytes
      // that's why we multiply tileLocation with  16
      // line is for jumping each row
      // Row0: 0x8000+(tileLocation*16)+0;
      // Row1: 0x8000+(tileLocation*16)+2;
      // ....
      // Row7: 0x8000+(tileLocation*16)+14;
      // So the reason i didn't write for 8x16 specific is that this match
      // automatically handle that too Row0: .......
      //.....
      // Row15:0x8000+(tileLocation*16)+30;
      word tileAddress = 0x8000 + (tileLocation * 16) + line;
      byte data1 = ReadMemory(tileAddress);
      byte data2 = ReadMemory(tileAddress + 1);

      // now start the horizontal pixel
      //  the reason backward is because of how the colorbit map to pixel bit
      //  same as tile colour bit
      for (int pixelbit = 7; pixelbit >= 0; pixelbit--) {
        int colorBit = pixelbit;
        if (flipX) { // flip it my boysss!!!!
          colorBit -= 7;
          colorBit *= -1;
        }
        // same as tile pixel
        int colorNum = ((data2 >> 1) & 1) << 1;
        colorNum |= (data1 << colorBit) & 1;
        word colorAddress = ((attributes & 16) != 0)
                                ? 0xFF49
                                : 0xFF48; // check the bit 4 of attributes
        COLOUR color = ReadColor(colorNum, colorAddress);
        if (color == WHITE) {
          // skip this current loop
          // why skip: the white is used for transparency
          continue;
        }
        int red = 0;
        int green = 0;
        int blue = 0;
        // only handle the two Enum
        // Cuz other two are already handled by default duhhhhhhhhhh
        switch (color) {
        case LIGHT_GRAY:
          red = 0xCC;
          green = 0xCC;
          blue = 0xCC;
          break;
        case DARK_GRAY:
          red = 0x77;
          green = 0x77;
          blue = 0x77;
          break;
        }

        // I wrote the pixle to be in reverse but when drawing has to be from 0
        // to 7 so for example ; if pixelbit is 7 that means the xPix= 0;
        int xPix = 7 - pixelbit;
        // current pixel
        int pixel = xPix + xPos;

        // boundry check
        if ((scanline < 0 || scanline > 143) || (pixel < 0 || pixel > 159)) {
          continue;
        }
        m_screenData[pixel][scanline][0] = red;
        m_screenData[pixel][scanline][1] = green;
        m_screenData[pixel][scanline][2] = blue;
      }
    }
  }
}

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
  byte joyP = m_rom[0xFF00]; // to know the group select
  // we only care about the higher nibles
  joyP |= 0x0F; // turn the lower 4 bit all 1
  // button case
  if ((joyP & 0x20) == 0) { // test the 5th bit
    // shift the upper bit to lower bit of joyPadState
    // as the joyPadState store the button bit on higher ones
    byte topjoyPad = (m_joyPadState >> 4) & 0x0F;
    joyP &= (0xF0 | topjoyPad);
  }
  // direction case
  if ((joyP & 0x10) == 0) { // test the 4th bit
    // doesn't need shift as direciton are already in lower bit
    byte lowJoyPad = m_joyPadState & 0xF; // turn of the higher bit if any set
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
  m_joyPadState &= ~(1 << key); // force to 0
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
};

// just force that bit to 1
void GameBoy::KeyReleased(int key) { m_joyPadState |= (1 << key); }

void GameBoy::NextOpCodeExcute() {
  int res = 0;
  if (m_Halt) {
    if (m_EIpending) {
      m_EIpending = false;
      m_MasterInterrupt = true;
    }
    return;
  }
  byte opcode = ReadMemory(m_programCounter);
  m_programCounter++;
  res = ExcuteOpcode(opcode);

  if (m_EIpending) {
    m_EIpending = false;
    m_MasterInterrupt = true;
  }
}

int GameBoy::ExcuteOpcode(byte opcode) {
  switch (opcode) {
  case 0x00:
    break;
  case 0x01:
    CPU_16bit_MemToReg(m_RegisterBC);
    return 12;
  case 0x02:
    CPU_8bit_RegToMem(m_RegisterBC, m_RegisterAF.hi, NONE);
    return 8;
  case 0x03:
    m_RegisterBC.reg++;
    return 8;
  case 0x04:
    CPU_8bit_SimOp(m_RegisterBC.hi, INC);
    return 4;
  case 0x05:
    CPU_8bit_SimOp(m_RegisterBC.hi, DEC);
    return 4;
  case 0x06:
    CPU_8bit_Load(m_RegisterBC.hi);
    return 8;
  case 0x07:
    CPU_8bit_RLC(m_RegisterAF.hi);
    return 4;
  case 0x08:
    CPU_16bit_RegToImmeMem(m_stackPointer);
    return 20;
  case 0x09:
    CPU_16bit_ADD(m_RegisterHL, m_RegisterBC, true);
    return 8;
  case 0x0A:
    CPU_8bit_MemToReg(m_RegisterAF.hi, m_RegisterBC, NONE);
    return 8;
  case 0x0B:
    m_RegisterBC.reg--;
    return 8;
  case 0x0C:
    CPU_8bit_SimOp(m_RegisterBC.lo, INC);
    return 4;
  case 0x0D:
    CPU_8bit_SimOp(m_RegisterBC.lo, DEC);
    return 4;
  case 0x0E:
    CPU_8bit_Load(m_RegisterBC.lo);
    return 8;
  case 0x0F:
    CPU_8bit_RRC(m_RegisterAF.hi);
    return 4;
  case 0x10:
    m_programCounter++;
    return 4;
  case 0x11:
    CPU_16bit_MemToReg(m_RegisterDE);
    return 12;
  case 0x12:
    CPU_8bit_RegToMem(m_RegisterDE, m_RegisterAF.hi, NONE);
    return 8;
  case 0x13:
    m_RegisterDE.reg++;
    return 8;
  case 0x14:
    CPU_8bit_SimOp(m_RegisterDE.hi, INC);
    return 4;
  case 0x15:
    CPU_8bit_SimOp(m_RegisterDE.hi, DEC);
    return 4;
  case 0x16:
    CPU_8bit_Load(m_RegisterDE.hi);
    return 8;
  case 0x17:
    CPU_8bit_RL(m_RegisterAF.hi);
    return 4;
  case 0x1A:
    CPU_8bit_MemToReg(m_RegisterAF.hi, m_RegisterDE, NONE);
    return 8;
  case 0x1B:
    m_RegisterDE.reg--;
    return 8;
  case 0x1C:
    CPU_8bit_SimOp(m_RegisterDE.lo, INC);
    return 4;
  case 0x1D:
    CPU_8bit_SimOp(m_RegisterDE.lo, DEC);
    return 4;
  case 0x1E:
    CPU_8bit_Load(m_RegisterDE.lo);
    return 8;
  case 0x1F:
    CPU_8bit_RR(m_RegisterAF.hi);
    return 4;
  case 0x19:
    CPU_16bit_ADD(m_RegisterHL, m_RegisterDE, true);
    return 8;
  case 0x21:
    CPU_16bit_MemToReg(m_RegisterHL);
    return 12;
  case 0x22:
    CPU_8bit_RegToMem(m_RegisterHL, m_RegisterAF.hi, INC);
    return 8;
  case 0x23:
    m_RegisterHL.reg++;
    return 8;
  case 0x24:
    CPU_8bit_SimOp(m_RegisterHL.hi, INC);
    return 4;
  case 0x25:
    CPU_8bit_SimOp(m_RegisterHL.hi, DEC);
    return 4;
  case 0x26:
    CPU_8bit_Load(m_RegisterHL.hi);
    return 8;
  case 0x27:
    CPU_8bit_DAA();
    return 4;
  case 0x2A:
    CPU_8bit_MemToReg(m_RegisterAF.hi, m_RegisterHL, INC);
    return 8;
  case 0x2B:
    m_RegisterHL.reg--;
    return 8;
  case 0x2C:
    CPU_8bit_SimOp(m_RegisterHL.lo, INC);
    return 4;
  case 0x2D:
    CPU_8bit_SimOp(m_RegisterHL.lo, DEC);
    return 4;
  case 0x2E:
    CPU_8bit_Load(m_RegisterHL.lo);
    return 8;
  case 0x2F:
    m_RegisterAF.hi ^= 0xFF;
    m_RegisterAF.lo |= (1 << FLAG_N) | (1 << FLAG_H);
    return 4;
  case 0x29:
    CPU_16bit_ADD(m_RegisterHL, m_RegisterHL, true);
    return 8;
  case 0x31:
    CPU_16bit_MemToReg(m_stackPointer);
    return 12;
  case 0x32:
    CPU_8bit_RegToMem(m_RegisterHL, m_RegisterAF.hi, DEC);
    return 8;
  case 0x33:
    m_stackPointer.reg++;
    return 8;
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
  case 0x3A:
    CPU_8bit_MemToReg(m_RegisterAF.hi, m_RegisterHL, DEC);
    return 8;
  case 0x3B:
    m_stackPointer.reg--;
    return 8;
  case 0x3C:
    CPU_8bit_SimOp(m_RegisterAF.hi, INC);
    return 4;
  case 0x3D:
    CPU_8bit_SimOp(m_RegisterAF.hi, DEC);
    return 4;
  case 0x3E:
    CPU_8bit_Load(m_RegisterAF.hi);
    return 8;
  case 0x39:
    CPU_16bit_ADD(m_RegisterHL, m_stackPointer, true);
    return 8;
  // Load register to register || Load Memory to Register
  case 0x7F:
    CPU_8bit_Reg_Load(m_RegisterAF.hi, m_RegisterAF.hi);
    return 4;
  case 0x77:
    CPU_8bit_RegToMem(m_RegisterHL, m_RegisterAF.hi, NONE);
    return 8;
  case 0x78:
    CPU_8bit_Reg_Load(m_RegisterAF.hi, m_RegisterBC.hi);
    return 4;
  case 0x79:
    CPU_8bit_Reg_Load(m_RegisterAF.hi, m_RegisterBC.lo);
    return 4;
  case 0x7A:
    CPU_8bit_Reg_Load(m_RegisterAF.hi, m_RegisterDE.hi);
    return 4;
  case 0x7B:
    CPU_8bit_Reg_Load(m_RegisterAF.hi, m_RegisterDE.lo);
    return 4;
  case 0x7C:
    CPU_8bit_Reg_Load(m_RegisterAF.hi, m_RegisterHL.hi);
    return 4;
  case 0x7D:
    CPU_8bit_Reg_Load(m_RegisterAF.hi, m_RegisterHL.lo);
    return 4;
  case 0x7E:
    CPU_8bit_MemToReg(m_RegisterAF.hi, m_RegisterHL, NONE);
    return 8;
  case 0x40:
    CPU_8bit_Reg_Load(m_RegisterBC.hi, m_RegisterBC.hi);
    return 4;
  case 0x41:
    CPU_8bit_Reg_Load(m_RegisterBC.hi, m_RegisterBC.lo);
    return 4;
  case 0x42:
    CPU_8bit_Reg_Load(m_RegisterBC.hi, m_RegisterDE.hi);
    return 4;
  case 0x43:
    CPU_8bit_Reg_Load(m_RegisterBC.hi, m_RegisterDE.lo);
    return 4;
  case 0x44:
    CPU_8bit_Reg_Load(m_RegisterBC.hi, m_RegisterHL.hi);
    return 4;
  case 0x45:
    CPU_8bit_Reg_Load(m_RegisterBC.hi, m_RegisterHL.lo);
    return 4;
  case 0x46:
    CPU_8bit_MemToReg(m_RegisterBC.hi, m_RegisterHL, NONE);
    return 8;
  case 0x47:
    CPU_8bit_Reg_Load(m_RegisterBC.hi, m_RegisterAF.hi);
    return 4;
  case 0x48:
    CPU_8bit_Reg_Load(m_RegisterBC.lo, m_RegisterBC.hi);
    return 4;
  case 0x49:
    CPU_8bit_Reg_Load(m_RegisterBC.lo, m_RegisterBC.lo);
    return 4;
  case 0x4A:
    CPU_8bit_Reg_Load(m_RegisterBC.lo, m_RegisterDE.hi);
    return 4;
  case 0x4B:
    CPU_8bit_Reg_Load(m_RegisterBC.lo, m_RegisterDE.lo);
    return 4;
  case 0x4C:
    CPU_8bit_Reg_Load(m_RegisterBC.lo, m_RegisterHL.hi);
    return 4;
  case 0x4D:
    CPU_8bit_Reg_Load(m_RegisterBC.lo, m_RegisterHL.lo);
    return 4;
  case 0x4E:
    CPU_8bit_MemToReg(m_RegisterBC.lo, m_RegisterHL, NONE);
    return 8;
  case 0x4F:
    CPU_8bit_Reg_Load(m_RegisterBC.lo, m_RegisterAF.hi);
    return 4;
  case 0x50:
    CPU_8bit_Reg_Load(m_RegisterDE.hi, m_RegisterBC.hi);
    return 4;
  case 0x51:
    CPU_8bit_Reg_Load(m_RegisterDE.hi, m_RegisterBC.lo);
    return 4;
  case 0x52:
    CPU_8bit_Reg_Load(m_RegisterDE.hi, m_RegisterDE.hi);
    return 4;
  case 0x53:
    CPU_8bit_Reg_Load(m_RegisterDE.hi, m_RegisterDE.lo);
    return 4;
  case 0x54:
    CPU_8bit_Reg_Load(m_RegisterDE.hi, m_RegisterHL.hi);
    return 4;
  case 0x55:
    CPU_8bit_Reg_Load(m_RegisterDE.hi, m_RegisterHL.lo);
    return 4;
  case 0x56:
    CPU_8bit_MemToReg(m_RegisterDE.hi, m_RegisterHL, NONE);
    return 8;
  case 0x57:
    CPU_8bit_Reg_Load(m_RegisterDE.hi, m_RegisterAF.hi);
    return 4;
  case 0x58:
    CPU_8bit_Reg_Load(m_RegisterDE.lo, m_RegisterBC.hi);
    return 4;
  case 0x59:
    CPU_8bit_Reg_Load(m_RegisterDE.lo, m_RegisterBC.lo);
    return 4;
  case 0x5A:
    CPU_8bit_Reg_Load(m_RegisterDE.lo, m_RegisterDE.hi);
    return 4;
  case 0x5B:
    CPU_8bit_Reg_Load(m_RegisterDE.lo, m_RegisterDE.lo);
    return 4;
  case 0x5C:
    CPU_8bit_Reg_Load(m_RegisterDE.lo, m_RegisterHL.hi);
    return 4;
  case 0x5D:
    CPU_8bit_Reg_Load(m_RegisterDE.lo, m_RegisterHL.lo);
    return 4;
  case 0x5E:
    CPU_8bit_MemToReg(m_RegisterDE.lo, m_RegisterHL, NONE);
    return 8;
  case 0x5F:
    CPU_8bit_Reg_Load(m_RegisterDE.lo, m_RegisterAF.hi);
    return 4;
  case 0x60:
    CPU_8bit_Reg_Load(m_RegisterHL.hi, m_RegisterBC.hi);
    return 4;
  case 0x61:
    CPU_8bit_Reg_Load(m_RegisterHL.hi, m_RegisterBC.lo);
    return 4;
  case 0x62:
    CPU_8bit_Reg_Load(m_RegisterHL.hi, m_RegisterDE.hi);
    return 4;
  case 0x63:
    CPU_8bit_Reg_Load(m_RegisterHL.hi, m_RegisterDE.lo);
    return 4;
  case 0x64:
    CPU_8bit_Reg_Load(m_RegisterHL.hi, m_RegisterHL.hi);
    return 4;
  case 0x65:
    CPU_8bit_Reg_Load(m_RegisterHL.hi, m_RegisterHL.lo);
    return 4;
  case 0x66:
    CPU_8bit_MemToReg(m_RegisterHL.hi, m_RegisterHL, NONE);
    return 8;
  case 0x67:
    CPU_8bit_Reg_Load(m_RegisterHL.hi, m_RegisterAF.hi);
    return 4;
  case 0x68:
    CPU_8bit_Reg_Load(m_RegisterHL.lo, m_RegisterBC.hi);
    return 4;
  case 0x69:
    CPU_8bit_Reg_Load(m_RegisterHL.lo, m_RegisterBC.lo);
    return 4;
  case 0x6A:
    CPU_8bit_Reg_Load(m_RegisterHL.lo, m_RegisterDE.hi);
    return 4;
  case 0x6B:
    CPU_8bit_Reg_Load(m_RegisterHL.lo, m_RegisterDE.lo);
    return 4;
  case 0x6C:
    CPU_8bit_Reg_Load(m_RegisterHL.lo, m_RegisterHL.hi);
    return 4;
  case 0x6D:
    CPU_8bit_Reg_Load(m_RegisterHL.lo, m_RegisterHL.lo);
    return 4;
  case 0x6E:
    CPU_8bit_MemToReg(m_RegisterHL.lo, m_RegisterHL, NONE);
    return 8;
  case 0x6F:
    CPU_8bit_Reg_Load(m_RegisterHL.lo, m_RegisterAF.hi);
    return 4;

  // Write Reg to Meme
  case 0x70:
    CPU_8bit_RegToMem(m_RegisterHL, m_RegisterBC.hi, NONE);
    return 8;
  case 0x71:
    CPU_8bit_RegToMem(m_RegisterHL, m_RegisterBC.lo, NONE);
    return 8;
  case 0x72:
    CPU_8bit_RegToMem(m_RegisterHL, m_RegisterDE.hi, NONE);
    return 8;
  case 0x73:
    CPU_8bit_RegToMem(m_RegisterHL, m_RegisterDE.lo, NONE);
    return 8;
  case 0x74:
    CPU_8bit_RegToMem(m_RegisterHL, m_RegisterHL.hi, NONE);
    return 8;
  case 0x75:
    CPU_8bit_RegToMem(m_RegisterHL, m_RegisterHL.lo, NONE);
    return 8;
  case 0x76:
    m_Halt = true;
    return 4;
  case 0x36:
    CPU_8bit_ImmeToMem(m_RegisterHL);
    return 12;

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

  // SBC A,r (subtract with carry)
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

  // AND A,r
  case 0xA0:
    CPU_8bit_AND(m_RegisterAF.hi, m_RegisterBC.hi, false);
    return 4;
  case 0xA1:
    CPU_8bit_AND(m_RegisterAF.hi, m_RegisterBC.lo, false);
    return 4;
  case 0xA2:
    CPU_8bit_AND(m_RegisterAF.hi, m_RegisterDE.hi, false);
    return 4;
  case 0xA3:
    CPU_8bit_AND(m_RegisterAF.hi, m_RegisterDE.lo, false);
    return 4;
  case 0xA4:
    CPU_8bit_AND(m_RegisterAF.hi, m_RegisterHL.hi, false);
    return 4;
  case 0xA5:
    CPU_8bit_AND(m_RegisterAF.hi, m_RegisterHL.lo, false);
    return 4;
  case 0xA6: {
    byte nn = ReadMemory(m_RegisterHL.reg);
    CPU_8bit_AND(m_RegisterAF.hi, nn, false);
    return 8;
  }
  case 0xA7:
    CPU_8bit_AND(m_RegisterAF.hi, m_RegisterAF.hi, false);
    return 4;

  // XOR A,r
  case 0xA8:
    CPU_8bit_XOR(m_RegisterAF.hi, m_RegisterBC.hi, false);
    return 4;
  case 0xA9:
    CPU_8bit_XOR(m_RegisterAF.hi, m_RegisterBC.lo, false);
    return 4;
  case 0xAA:
    CPU_8bit_XOR(m_RegisterAF.hi, m_RegisterDE.hi, false);
    return 4;
  case 0xAB:
    CPU_8bit_XOR(m_RegisterAF.hi, m_RegisterDE.lo, false);
    return 4;
  case 0xAC:
    CPU_8bit_XOR(m_RegisterAF.hi, m_RegisterHL.hi, false);
    return 4;
  case 0xAD:
    CPU_8bit_XOR(m_RegisterAF.hi, m_RegisterHL.lo, false);
    return 4;
  case 0xAE: {
    byte nn = ReadMemory(m_RegisterHL.reg);
    CPU_8bit_XOR(m_RegisterAF.hi, nn, false);
    return 8;
  }
  case 0xAF:
    CPU_8bit_XOR(m_RegisterAF.hi, m_RegisterAF.hi, false);
    return 4;

  // XOR A,n (immediate)
  case 0xEE:
    CPU_8bit_XOR(m_RegisterAF.hi, 0, true);
    return 8;

  // OR A,r
  case 0xB0:
    CPU_8bit_OR(m_RegisterAF.hi, m_RegisterBC.hi, false);
    return 4;
  case 0xB1:
    CPU_8bit_OR(m_RegisterAF.hi, m_RegisterBC.lo, false);
    return 4;
  case 0xB2:
    CPU_8bit_OR(m_RegisterAF.hi, m_RegisterDE.hi, false);
    return 4;
  case 0xB3:
    CPU_8bit_OR(m_RegisterAF.hi, m_RegisterDE.lo, false);
    return 4;
  case 0xB4:
    CPU_8bit_OR(m_RegisterAF.hi, m_RegisterHL.hi, false);
    return 4;
  case 0xB5:
    CPU_8bit_OR(m_RegisterAF.hi, m_RegisterHL.lo, false);
    return 4;
  case 0xB6: {
    byte nn = ReadMemory(m_RegisterHL.reg);
    CPU_8bit_OR(m_RegisterAF.hi, nn, false);
    return 8;
  }
  case 0xB7:
    CPU_8bit_OR(m_RegisterAF.hi, m_RegisterAF.hi, false);
    return 4;

  // CP A,r
  case 0xB8:
    CPU_8bit_CP(m_RegisterAF.hi, m_RegisterBC.hi);
    return 4;
  case 0xB9:
    CPU_8bit_CP(m_RegisterAF.hi, m_RegisterBC.lo);
    return 4;
  case 0xBA:
    CPU_8bit_CP(m_RegisterAF.hi, m_RegisterDE.hi);
    return 4;
  case 0xBB:
    CPU_8bit_CP(m_RegisterAF.hi, m_RegisterDE.lo);
    return 4;
  case 0xBC:
    CPU_8bit_CP(m_RegisterAF.hi, m_RegisterHL.hi);
    return 4;
  case 0xBD:
    CPU_8bit_CP(m_RegisterAF.hi, m_RegisterHL.lo);
    return 4;
  case 0xBE: {
    byte nn = ReadMemory(m_RegisterHL.reg);
    CPU_8bit_CP(m_RegisterAF.hi, nn);
    return 8;
  }
  case 0xBF:
    CPU_8bit_CP(m_RegisterAF.hi, m_RegisterAF.hi);
    return 4;

  // CP A,n (immediate)
  case 0xFE: {
    byte nn = ReadMemory(m_programCounter);
    m_programCounter++;
    CPU_8bit_CP(m_RegisterAF.hi, nn);
    return 8;
  }

  // JR cc,n
  case 0x18:
    CPU_JUMP_IMMEDIATE(false, 0, false);
    return 8;
  case 0x20:
    CPU_JUMP_IMMEDIATE(false, FLAG_Z, true);
    return 8;
  case 0x28:
    CPU_JUMP_IMMEDIATE(true, FLAG_Z, true);
    return 8;
  case 0x30:
    CPU_JUMP_IMMEDIATE(false, FLAG_C, true);
    return 8;
  case 0x38:
    CPU_JUMP_IMMEDIATE(true, FLAG_C, true);
    return 8;
  case 0x3F:
    m_RegisterAF.lo &= ~(1 << FLAG_N);
    m_RegisterAF.lo &= ~(1 << FLAG_H);
    m_RegisterAF.lo ^= (1 << FLAG_C);
    return 4;
    // CALL
  case 0xC1:
    CPU_16bit_PopToReg(m_RegisterBC);
    return 12;
  case 0xC2:
    CPU_8bit_JP_2Byte_Imme(NZ);
    return 12;
  case 0xC3:
    m_programCounter = ReadWord();
    return 12;
  case 0xC4:
    CPU_Call(false, FLAG_Z, true);
    return 12;
  case 0xC5:
    PushWordToStack(m_RegisterBC.reg);
    return 16;
  case 0xC6:
    CPU_8bit_ADD(m_RegisterAF.hi, 0, true, false);
    return 8;
  case 0xC7:
    CPU_8bit_Restart(0x00);
    return 32;
  case 0xCE:
    CPU_8bit_ADD(m_RegisterAF.hi, 0, true, true);
    return 8;
  case 0xCC:
    CPU_Call(true, FLAG_Z, true);
    return 12;
  case 0xCD:
    CPU_Call(false, FLAG_C, false);
    return 12;
  case 0xCA:
    CPU_8bit_JP_2Byte_Imme(Z);
    return 12;

  case 0xCB: {
    byte cb_opcode = ReadMemory(m_programCounter);
    m_programCounter++;
    switch (cb_opcode) {
    case 0x00:
      CPU_8bit_RLC(m_RegisterBC.hi);
      return 8;
    case 0x01:
      CPU_8bit_RLC(m_RegisterBC.lo);
      return 8;
    case 0x02:
      CPU_8bit_RLC(m_RegisterDE.hi);
      return 8;
    case 0x03:
      CPU_8bit_RLC(m_RegisterDE.lo);
      return 8;
    case 0x04:
      CPU_8bit_RLC(m_RegisterHL.hi);
      return 8;
    case 0x05:
      CPU_8bit_RLC(m_RegisterHL.lo);
      return 8;
    case 0x06: {
      byte val = ReadMemory(m_RegisterHL.reg);
      CPU_8bit_RLC(val);
      WriteMemory(m_RegisterHL.reg, val);
      return 16;
    }
    case 0x07:
      CPU_8bit_RLC(m_RegisterAF.hi);
      return 8;
    case 0x08:
      CPU_8bit_RRC(m_RegisterBC.hi);
      return 8;
    case 0x09:
      CPU_8bit_RRC(m_RegisterBC.lo);
      return 8;
    case 0x0A:
      CPU_8bit_RRC(m_RegisterDE.hi);
      return 8;
    case 0x0B:
      CPU_8bit_RRC(m_RegisterDE.lo);
      return 8;
    case 0x0C:
      CPU_8bit_RRC(m_RegisterHL.hi);
      return 8;
    case 0x0D:
      CPU_8bit_RRC(m_RegisterHL.lo);
      return 8;
    case 0x0E: {
      byte val = ReadMemory(m_RegisterHL.reg);
      CPU_8bit_RRC(val);
      WriteMemory(m_RegisterHL.reg, val);
      return 16;
    }
    case 0x0F:
      CPU_8bit_RRC(m_RegisterAF.hi);
      return 8;
    case 0x10:
      CPU_8bit_RL(m_RegisterBC.hi);
      return 8;
    case 0x11:
      CPU_8bit_RL(m_RegisterBC.lo);
      return 8;
    case 0x12:
      CPU_8bit_RL(m_RegisterDE.hi);
      return 8;
    case 0x13:
      CPU_8bit_RL(m_RegisterDE.lo);
      return 8;
    case 0x14:
      CPU_8bit_RL(m_RegisterHL.hi);
      return 8;
    case 0x15:
      CPU_8bit_RL(m_RegisterHL.lo);
      return 8;
    case 0x16: {
      byte val = ReadMemory(m_RegisterHL.reg);
      CPU_8bit_RL(val);
      WriteMemory(m_RegisterHL.reg, val);
      return 16;
    }
    case 0x17:
      CPU_8bit_RL(m_RegisterAF.hi);
      return 8;
    case 0x18:
      CPU_8bit_RR(m_RegisterBC.hi);
      return 8;
    case 0x19:
      CPU_8bit_RR(m_RegisterBC.lo);
      return 8;
    case 0x1A:
      CPU_8bit_RR(m_RegisterDE.hi);
      return 8;
    case 0x1B:
      CPU_8bit_RR(m_RegisterDE.lo);
      return 8;
    case 0x1C:
      CPU_8bit_RR(m_RegisterHL.hi);
      return 8;
    case 0x1D:
      CPU_8bit_RR(m_RegisterHL.lo);
      return 8;
    case 0x1E: {
      byte val = ReadMemory(m_RegisterHL.reg);
      CPU_8bit_RR(val);
      WriteMemory(m_RegisterHL.reg, val);
      return 16;
    }
    case 0x1F:
      CPU_8bit_RR(m_RegisterAF.hi);
      return 8;
    case 0x20:
      CPU_8bit_SLA(m_RegisterBC.hi);
      return 8;
    case 0x21:
      CPU_8bit_SLA(m_RegisterBC.lo);
      return 8;
    case 0x22:
      CPU_8bit_SLA(m_RegisterDE.hi);
      return 8;
    case 0x23:
      CPU_8bit_SLA(m_RegisterDE.lo);
      return 8;
    case 0x24:
      CPU_8bit_SLA(m_RegisterHL.hi);
      return 8;
    case 0x25:
      CPU_8bit_SLA(m_RegisterHL.lo);
      return 8;
    case 0x26: {
      byte val = ReadMemory(m_RegisterHL.reg);
      CPU_8bit_SLA(val);
      WriteMemory(m_RegisterHL.reg, val);
      return 16;
    }
    case 0x27:
      CPU_8bit_SLA(m_RegisterAF.hi);
      return 8;
    case 0x28:
      CPU_8bit_SRA(m_RegisterBC.hi);
      return 8;
    case 0x29:
      CPU_8bit_SRA(m_RegisterBC.lo);
      return 8;
    case 0x2A:
      CPU_8bit_SRA(m_RegisterDE.hi);
      return 8;
    case 0x2B:
      CPU_8bit_SRA(m_RegisterDE.lo);
      return 8;
    case 0x2C:
      CPU_8bit_SRA(m_RegisterHL.hi);
      return 8;
    case 0x2D:
      CPU_8bit_SRA(m_RegisterHL.lo);
      return 8;
    case 0x2E: {
      byte val = ReadMemory(m_RegisterHL.reg);
      CPU_8bit_SRA(val);
      WriteMemory(m_RegisterHL.reg, val);
      return 16;
    }
    case 0x2F:
      CPU_8bit_SRA(m_RegisterAF.hi);
      return 8;
    case 0x30:
      CPU_8bit_SWAP(m_RegisterBC.hi);
      return 8;
    case 0x31:
      CPU_8bit_SWAP(m_RegisterBC.lo);
      return 8;
    case 0x32:
      CPU_8bit_SWAP(m_RegisterDE.hi);
      return 8;
    case 0x33:
      CPU_8bit_SWAP(m_RegisterDE.lo);
      return 8;
    case 0x34:
      CPU_8bit_SWAP(m_RegisterHL.hi);
      return 8;
    case 0x35:
      CPU_8bit_SWAP(m_RegisterHL.lo);
      return 8;
    case 0x36: {
      byte val = ReadMemory(m_RegisterHL.reg);
      CPU_8bit_SWAP(val);
      WriteMemory(m_RegisterHL.reg, val);
      return 16;
    }
    case 0x37:
      CPU_8bit_SWAP(m_RegisterAF.hi);
      return 8;
    case 0x38:
      CPU_8bit_SRL(m_RegisterBC.hi);
      return 8;
    case 0x39:
      CPU_8bit_SRL(m_RegisterBC.lo);
      return 8;
    case 0x3A:
      CPU_8bit_SRL(m_RegisterDE.hi);
      return 8;
    case 0x3B:
      CPU_8bit_SRL(m_RegisterDE.lo);
      return 8;
    case 0x3C:
      CPU_8bit_SRL(m_RegisterHL.hi);
      return 8;
    case 0x3D:
      CPU_8bit_SRL(m_RegisterHL.lo);
      return 8;
    case 0x3E: {
      byte val = ReadMemory(m_RegisterHL.reg);
      CPU_8bit_SRL(val);
      WriteMemory(m_RegisterHL.reg, val);
      return 16;
    }
    case 0x3F:
      CPU_8bit_SRL(m_RegisterAF.hi);
      return 8;
    default:
      // BIT b,r (0x40-0x7F) — test bit b in register r
      // register field (bits 2-0) = 6 means (HL) → 16 cycles
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
    // RETURN
  case 0xC8:
    CPU_RETURN(true, FLAG_Z, true);
    return 8;
  case 0xC9: {
    word addr = PopWordFromStack();
    m_programCounter = addr;
    return 8;
  }
  case 0xC0:
    CPU_RETURN(false, FLAG_Z, true);
    return 8;
  case 0xD0:
    CPU_RETURN(false, FLAG_C, true);
    return 8;
  case 0xD1:
    CPU_16bit_PopToReg(m_RegisterDE);
    return 12;
  case 0xD2:
    CPU_8bit_JP_2Byte_Imme(NC);
    return 12;
  case 0xD4:
    CPU_Call(false, FLAG_C, true);
    return 12;
  case 0xD5:
    PushWordToStack(m_RegisterDE.reg);
    return 16;
  case 0xD6:
    CPU_8bit_SUB(m_RegisterAF.hi, 0, true, false);
    return 8;
  case 0xD8:
    CPU_RETURN(true, FLAG_C, true);
    return 8;
  case 0xD9: {
    word addr = PopWordFromStack();
    m_programCounter = addr;
    m_MasterInterrupt = true;
    m_EIpending = false;
    return 8;
  }

  case 0xDA:
    CPU_8bit_JP_2Byte_Imme(C);
    return 12;
  case 0xDC:
    CPU_Call(true, FLAG_C, true);
    return 12;
  case 0xDE:
    CPU_8bit_SUB(m_RegisterAF.hi, 0, true, true);
    return 8;
  case 0xE0:
    CPU_8bit_RegToImmeN0xFF00(m_RegisterAF.hi);
    return 12;
  case 0xE1:
    CPU_16bit_PopToReg(m_RegisterHL);
    return 12;
  case 0xE2:
    CPU_8bit_RegToC(m_RegisterAF.hi);
    return 8;
  case 0xE5:
    PushWordToStack(m_RegisterHL.reg);
    return 16;
  case 0xE6:
    CPU_8bit_AND(m_RegisterAF.hi, 0, true);
    return 8;
  case 0xE8:
    CPU_16bit_NToSP();
    return 16;
  case 0xE9:
    m_programCounter = m_RegisterHL.reg;
    return 4;
  case 0xEA:
    CPU_8bit_RegToImmeMem(m_RegisterAF.hi);
    return 12;
  case 0xF0:
    CPU_8bit_ImmeN0xFF00ToReg(m_RegisterAF.hi);
    return 12;
  case 0xF1:
    CPU_16bit_PopToReg(m_RegisterAF);
    return 12;
  case 0xF2:
    CPU_8bit_CToReg(m_RegisterAF.hi);
    return 8;
  case 0xF3:
    m_MasterInterrupt = false;
    m_EIpending = false;
    return 4;
  case 0xF5:
    PushWordToStack(m_RegisterAF.reg);
    return 16;
  case 0xF6:
    CPU_8bit_OR(m_RegisterAF.hi, 0, true);
    return 8;
  case 0xF8:
    CPU_16bit_SPNnToHL();
    return 12;
  case 0xF9:
    CPU_16bit_Reg_Load(m_stackPointer, m_RegisterHL);
    return 8;
  case 0xFA:
    CPU_8bit_ImmeMemToReg(m_RegisterAF.hi);
    return 12;
  case 0xFB:
    m_EIpending = true;
    return 4;
  case 0xCF:
    CPU_8bit_Restart(0x08);
    return 32;
  case 0xD7:
    CPU_8bit_Restart(0x10);
    return 32;
  case 0xDF:
    CPU_8bit_Restart(0x18);
    return 32;
  case 0xE7:
    CPU_8bit_Restart(0x20);
    return 32;
  case 0xEF:
    CPU_8bit_Restart(0x28);
    return 32;
  case 0xF7:
    CPU_8bit_Restart(0x30);
    return 32;
  case 0xFF:
    CPU_8bit_Restart(0x38);
    return 32;
  default:
    return 0;
  }
}
// Load nn to n
// Load n value to registerBC(high)
void GameBoy::CPU_8bit_Load(byte &reg) {
  byte n = ReadMemory(m_programCounter);
  m_programCounter++;
  reg = n;
}

void GameBoy::CPU_8bit_Reg_Load(byte &reg1, byte &reg2) { reg1 = reg2; };
void GameBoy::CPU_16bit_Reg_Load(Register &reg1, Register &reg2) {
  reg1 = reg2;
};

// Add n to A
// n can be register or value
void GameBoy::CPU_8bit_ADD(byte &reg, byte toAdd, bool useImmediate,
                           bool addCarry) {
  byte before = reg;
  byte adding = 0;

  // if  using immediate(which means no value from the register)
  // we have to load from the catridge memory
  if (useImmediate) {
    byte n = ReadMemory(m_programCounter);
    m_programCounter++;
    adding += n;
  } else {
    adding = toAdd;
  }
  // if there is an carry flag turned on
  if (addCarry) {
    if ((m_RegisterAF.lo & (1 << FLAG_C)) != 0) {
      adding++;
    }
  }
  reg += adding;
  // reset the flag
  m_RegisterAF.lo = 0;
  // check flag z
  if (reg == 0) {
    m_RegisterAF.lo |= 1 << FLAG_Z;
  }
  // only take the lower nibbles as it is half carry
  byte hCheck = before & 0xF;
  hCheck += adding & 0xF;
  if (hCheck > 0xF) { // if gt 16
    m_RegisterAF.lo |= 1 << FLAG_H;
  }
  // the reason we check before+adding is because if we chekc the reg it will
  // already be in overflow state
  if (before + adding > 0xFF) { // check full carry
    m_RegisterAF.lo |= 1 << FLAG_C;
  }
}

void GameBoy::CPU_8bit_SUB(byte &reg, byte toSub, bool useImmediate,
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
  if (borrowCarry) {
    if ((m_RegisterAF.lo & (1 << FLAG_C)) != 0) {
      subbing += 1;
    }
  }
  reg -= subbing;
  m_RegisterAF.lo = 0;
  if (reg == 0) {
    m_RegisterAF.lo |= (1 << FLAG_Z);
  }
  // turn on the sub flag
  m_RegisterAF.lo |= (1 << FLAG_N);

  // half borrow check
  if ((before & 0xF) < (subbing & 0xF)) {
    m_RegisterAF.lo |= (1 << FLAG_H);
  }
  // full borrow check
  if ((before & 0xFF) < (subbing & 0xFF)) {
    m_RegisterAF.lo |= (1 << FLAG_C);
  }
}

void GameBoy::CPU_8bit_XOR(byte &reg, byte toXOR, bool useImmediate) {
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
};

void GameBoy::CPU_8bit_AND(byte &reg, byte toAND, bool useImmediate) {
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
  // as the AND opeartion have the default H flag turned on
  m_RegisterAF.lo |= (1 << FLAG_H);

  if (reg == 0) {
    m_RegisterAF.lo |= (1 << FLAG_Z);
  }
};

void GameBoy::CPU_8bit_OR(byte &reg, byte toOR, bool useImmediate) {
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
};

void GameBoy::CPU_JUMP_IMMEDIATE(bool condition, int flag, bool useCondition) {
  signed char n = (signed_byte)ReadMemory(m_programCounter);
  if (!useCondition) {
    // if not using condition/jump straight
    m_programCounter += n;
  } else if ((((m_RegisterAF.lo & (1 << flag)) != 0) ? true : false) ==
             condition) {
    // check if the flag register is true with the condition
    m_programCounter += n;
  }
  m_programCounter++;
};

void GameBoy::CPU_Call(bool condition, int flag, bool useCondition) {
  byte word = ReadWord();
  m_programCounter += 2;
  if (!useCondition) {
    PushWordToStack(word);
    m_programCounter = word;
    return;
  }
  if (((m_RegisterAF.lo & (1 << flag)) != 0 ? true : false) == condition) {
    PushWordToStack(word);
    m_programCounter = word;
  }
}

word GameBoy::ReadWord() {
  byte low = ReadMemory(m_programCounter);

  byte high = ReadMemory(m_programCounter + 1);
  return (high << 8) | low;
}

void GameBoy::CPU_RETURN(bool condition, int flag, bool useCondition) {
  if (!useCondition) {
    m_programCounter = PopWordFromStack();
    return;
  }

  if ((((m_RegisterAF.lo & (1 << FLAG_Z)) != 0) ? true : false) == condition) {
    m_programCounter = PopWordFromStack();
  }
}

void GameBoy::CPU_8bit_MemToReg(byte &reg1, Register reg2, OP operation) {
  // register stores separte in 8bit
  word addr = (reg2.hi << 8);
  addr |= reg2.lo;
  byte value = ReadMemory(addr);
  reg1 = value;
  switch (operation) {
  case NONE:
    break;
  case INC:
    m_RegisterHL.reg++;
    break;
  case DEC:
    m_RegisterHL.reg--;
  }
};

void GameBoy::CPU_8bit_RegToMem(Register reg1, byte reg2, OP operation) {
  word addr = (reg1.hi << 8) | reg1.lo;
  WriteMemory(addr, reg2);
  switch (operation) {
  case NONE:
    break;
  case INC:
    m_RegisterHL.reg++;
    break;
  case DEC:
    m_RegisterHL.reg--;
  }
};

void GameBoy::CPU_8bit_ImmeToMem(Register reg1) {
  byte n = ReadMemory(m_programCounter);
  word addr = (reg1.hi << 8) | reg1.lo;
  WriteMemory(addr, n);
  m_programCounter++;
}

void GameBoy::CPU_8bit_ImmeMemToReg(byte &reg1) {
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

void GameBoy::CPU_8bit_ImmeN0xFF00ToReg(byte &reg) {
  byte n = ReadMemory(m_programCounter);
  m_programCounter++;
  byte data = ReadMemory(0xFF00 + n);
  reg = data;
}

void GameBoy::CPU_8bit_CToReg(byte &reg) {
  word data = ReadMemory(0xFF00 | m_RegisterBC.lo);
  reg = data;
}

void GameBoy::CPU_16bit_MemToReg(Register &reg) {
  word nn = ReadWord();
  m_programCounter += 2;
  reg.reg = nn;
};

void GameBoy::CPU_16bit_PopToReg(Register &reg) {
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
};

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
void GameBoy::CPU_8bit_INC(byte &reg, byte &flagReg) {
  reg++;
  flagReg &= (1 << FLAG_C);
  if (reg == 0) {
    flagReg |= (1 << FLAG_Z);
  }
  if ((reg & 0xF) == 0) {
    flagReg |= (1 << FLAG_H);
  }
}

void GameBoy::CPU_8bit_DEC(byte &reg, byte &flagReg) {
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
void GameBoy::CPU_8bit_SimOp(byte &reg, OP operation) {
  switch (operation) {
  case INC:
    CPU_8bit_INC(reg, m_RegisterAF.lo);
    break;
  case DEC:
    CPU_8bit_DEC(reg, m_RegisterAF.lo);
    break;
  default:
    break;
  }
}

void GameBoy::CPU_16bit_ADD(Register &reg, Register reg2, bool z_flag) {
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

void GameBoy::CPU_8bit_SWAP(byte &reg) {
  reg = (reg << 4) | (reg >> 4);
  m_RegisterAF.lo = 0;
  if (reg == 0) {
    m_RegisterAF.lo |= (1 << FLAG_Z);
  }
}
// This function turn the hex to decimal so that the game can do the correct
// binary decimal representation of score and stuff
void GameBoy::CPU_8bit_DAA() {
  byte &a = m_RegisterAF.hi;
  byte &f = m_RegisterAF.lo;

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

void GameBoy::CPU_8bit_RLC(byte &reg) {
  byte val = (reg >> 7) & 1;
  reg = (reg << 1) | val;
  m_RegisterAF.lo = 0;
  if (reg == 0) {
    m_RegisterAF.lo |= (1 << FLAG_Z);
  }
  m_RegisterAF.lo |= (val << FLAG_C);
}

void GameBoy::CPU_8bit_RL(byte &reg) {
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

void GameBoy::CPU_8bit_RRC(byte &reg) {
  byte val = reg & 1;
  reg = (reg >> 1) | (val << 7);
  m_RegisterAF.lo = 0;
  if (reg == 0) {
    m_RegisterAF.lo |= (1 << FLAG_Z);
  }
  m_RegisterAF.lo |= (val << FLAG_C);
}

void GameBoy::CPU_8bit_RR(byte &reg) {
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

void GameBoy::CPU_8bit_SLA(byte &reg) {
  byte old_bit7 = (reg >> 7) & 1;
  reg = reg << 1;
  m_RegisterAF.lo = 0;
  if (reg == 0) {
    m_RegisterAF.lo |= (1 << FLAG_Z);
  }
  m_RegisterAF.lo |= (old_bit7 << FLAG_C);
}

void GameBoy::CPU_8bit_SRA(byte &reg) {
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
};

void GameBoy::CPU_8bit_SRL(byte &reg) {
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
  case 0:
    val = m_RegisterBC.hi;
    break;
  case 1:
    val = m_RegisterBC.lo;
    break;
  case 2:
    val = m_RegisterDE.hi;
    break;
  case 3:
    val = m_RegisterDE.lo;
    break;
  case 4:
    val = m_RegisterHL.hi;
    break;
  case 5:
    val = m_RegisterHL.lo;
    break;
  case 6:
    val = ReadMemory(m_RegisterHL.reg);
    break;
  case 7:
    val = m_RegisterAF.hi;
    break;
  }
  m_RegisterAF.lo &= (1 << FLAG_C); // not effected
  m_RegisterAF.lo |= (1 << FLAG_H); // set
  if (!(val & (1 << bit))) {
    m_RegisterAF.lo |= (1 << FLAG_Z);
  }
}

void GameBoy::CPU_8bit_BIT_SET(byte opcode) {
  byte bit = (opcode >> 3) & 7;
  byte reg = opcode & 7;
  switch (reg) {
  case 0:
    m_RegisterBC.hi |= (1 << bit);
    break;
  case 1:
    m_RegisterBC.lo |= (1 << bit);
    break;
  case 2:
    m_RegisterDE.hi |= (1 << bit);
    break;
  case 3:
    m_RegisterDE.lo |= (1 << bit);
    break;
  case 4:
    m_RegisterHL.hi |= (1 << bit);
    break;
  case 5:
    m_RegisterHL.lo |= (1 << bit);
    break;
  case 6: {
    byte val = ReadMemory(m_RegisterHL.reg);
    val |= (1 << bit);
    WriteMemory(m_RegisterHL.reg, val);
    break;
  }
  case 7:
    m_RegisterAF.hi |= (1 << bit);
    break;
  }
}

void GameBoy::CPU_8bit_BIT_RESET(byte opcode) {
  byte bit = (opcode >> 3) & 7;
  byte reg = opcode & 7;
  switch (reg) {
  case 0:
    m_RegisterBC.hi &= ~(1 << bit);
    break;
  case 1:
    m_RegisterBC.lo &= ~(1 << bit);
    break;
  case 2:
    m_RegisterDE.hi &= ~(1 << bit);
    break;
  case 3:
    m_RegisterDE.lo &= ~(1 << bit);
    break;
  case 4:
    m_RegisterHL.hi &= ~(1 << bit);
    break;
  case 5:
    m_RegisterHL.lo &= ~(1 << bit);
    break;
  case 6: {
    byte val = ReadMemory(m_RegisterHL.reg);
    val &= ~(1 << bit);
    WriteMemory(m_RegisterHL.reg, val);
    break;
  }
  case 7:
    m_RegisterAF.hi &= ~(1 << bit);
    break;
  }
}

void GameBoy::CPU_8bit_JP_2Byte_Imme(CC cc) {
  word addr = ReadWord();
  switch (cc) {
  case 0:
    if ((m_RegisterAF.lo & (1 << FLAG_Z)) == 0) {
      m_programCounter = addr;
    } else {
      m_programCounter += 2;
    }
    break;
  case 1:
    if ((m_RegisterAF.lo & (1 << FLAG_Z)) != 0) {
      m_programCounter = addr;
    } else {
      m_programCounter += 2;
    }
    break;
  case 2:
    if ((m_RegisterAF.lo & (1 << FLAG_C)) == 0) {
      m_programCounter = addr;
    } else {
      m_programCounter += 2;
    }
    break;
  case 3:
    if ((m_RegisterAF.lo & (1 << FLAG_C)) != 0) {
      m_programCounter = addr;
    } else {
      m_programCounter += 2;
    }
    break;
  default:
    m_programCounter += 2;
    break;
  }
}

void GameBoy::CPU_8bit_Restart(byte addr) {
  PushWordToStack(m_programCounter);
  m_programCounter = addr;
}
