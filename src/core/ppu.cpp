#include "../utils/Debug.hpp"
#include "GameBoy.hpp"
// Timer address
#define TIMA 0xFF05
#define TMA 0xFF06
#define TMC 0xFF07

// Updae the scanline lcd
// The GameBoy has 160x144
// Scanline start from 0 to 153
// 144 visible scanline && 10 invisible scanline
// Takes 456 cycles for 1 scanline to finish
void GameBoy::UpdateGraphics(int cycles) {
  if (LCD_enabled()) {
    m_scalineCounter -= cycles;
    if (m_scalineCounter <= 0) {  // time to move to the next line
      byte currentLine = ReadMemory(0xFF44);
      m_scalineCounter = 456;  // reset the cycle count
      m_hdmaLineDone = false;  // next line can take its HBlank chunk
      if (currentLine < 144) {
        DrawScanLine();
      }

      m_rom[0xFF44]++;
      currentLine = ReadMemory(0xFF44);
      if (currentLine == 144) {  // about to enter v-blank
        RequestInterrupt(0);
      } else if (currentLine > 153) {  // reset to the top
        m_rom[0xFF44] = 0;
      }
    }
    SetLCD_status();
  }
  // HBlank DMA: one 16-byte chunk per HBlank while mode 0 is active
  if (m_hdmaActive && m_hdmaHBlankMode) {
    // only during the mode-0 and mode-2
    // not mode-1(V-blank)
    if ((ReadMemory(0xFF41) & 0x3) == 0 && ReadMemory(0xFF44) < 144 &&
        !m_hdmaLineDone) {
      DoHDMAChunk();
      m_hdmaLineDone = true;
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
void GameBoy::SetLCD_status() {
  byte status = ReadMemory(0xFF41);
  if (LCD_enabled() == false) {
    m_scalineCounter = 456;
    m_rom[0xFF44] = 0;  // scaline has to set to  0
    status &= 252;      // make the first two bit to 0
    status |= 1;        // turn on the first bit
    status &= ~4;
    m_previousStatusLine = false;
    WriteMemory(0xFF41, status);
    return;
  }
  byte currentLine = ReadMemory(0xFF44);
  byte currentMode = status & 0x3;
  byte mode = 0;

  // v-blank situation
  if (currentLine >= 144) {
    mode = 1;  // v-blank
  } else {
    int mode2Counts = 456 - 82;           // mode 2 use  82cycles
    int mode3Counts = mode2Counts - 172;  // mode 3 use 172 cycles

    // search sprite situation
    if (m_scalineCounter >= mode2Counts) {
      mode = 2;
    }

    // transfer data to lcd driver situation
    else if (m_scalineCounter >= mode3Counts) {
      mode = 3;
      // mode 3 doesn't have interrupt
    } else {  // h-blank situation
      mode = 0;
    }
  }
  status = (status & 252) | mode;
  bool lyc_match = (currentLine == ReadMemory(0xFF45));
  if (lyc_match) {  // coincidence
    status |= 4;    // turn on the 2th bit
  } else {
    status &= ~4;  // turn off  bit 2
  }
  bool lyc_ie = ((status & (1 << 6)) != 0);
  bool mode2_ie = ((status & (1 << 5)) != 0);
  bool mode1_ie = ((status & (1 << 4)) != 0);
  bool mode0_ie = ((status & (1 << 3)) != 0);

  bool mode2_act = (mode == 2);
  bool mode1_act = (mode == 1);
  bool mode0_act = (mode == 0);

  bool status_line = (lyc_match && lyc_ie) || (mode2_act && mode2_ie) ||
                     (mode1_act && mode1_ie) || (mode0_act && mode0_ie);
  if (status_line && !m_previousStatusLine) {
    RequestInterrupt(1);  // request LCD interrupt
  }
  m_previousStatusLine = status_line;
  WriteMemory(0xFF41, status);
}

// GameBoy doesn't allow the sprite ram(OAM) to update and delete when it is
// drawing. Only allow during v-blank, so to update during that v-blank period
// it is not enough time to update whole in that scenario, devs use dma DMA
// destination address(0xFE00-0xFE9F) which is exacly 0xA0 byte
void GameBoy::DoDMATransfer(byte address) {
  // 0xFF46 has the source address and only store 8 bit
  word targetAddr = address << 8;   // shift by right 8 to form 16bit
  for (int i = 0; i < 0xA0; i++) {  // 160
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
  if (m_isGBC || (status & 1) != 0) {  // check bit 0
    RenderTiles();
  }

  if ((status & 2) != 0) {  // check bit 1
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
  byte status = ReadMemory(0xFF40);
  byte scanline = ReadMemory(0xFF44);
  bool unsig = (status & 16) != 0;
  byte scrollY = ReadMemory(0xFF42);
  byte scrollX = ReadMemory(0xFF43);
  byte windowY = ReadMemory(0xFF4A);
  int windowX =
      (int)ReadMemory(0xFF4B) - 7;  // in order for the ppu to prefetch the data

  // draw the horizontal pixel
  for (int pixel = 0; pixel < 160; pixel++) {
    bool windowEnabled = (status & 0x20) != 0;
    bool isWindowPixel =
        windowEnabled && (scanline >= windowY) && (pixel >= windowX);
    byte xPos = 0;
    byte localY = 0;

    if (isWindowPixel) {  // at drawing window pixel
      int winX = pixel - windowX;
      xPos = (winX < 0) ? 0 : (byte)(winX);
      localY = scanline - windowY;
      // whcih window memory region
      backgroundMem = (status & 0x40) ? 0x9C00 : 0x9800;
    } else {
      xPos = scrollX + pixel;
      localY = scanline + scrollY;
      backgroundMem = (status & 0x08) ? 0x9C00 : 0x9800;
    }

    // horizontal tile index
    word tileCol = xPos / 8;
    // each tile is 8x8
    // so to get the tileRow index(current scanline pixel) we have to divide by
    // 8 to updated localY and multiply 32 to jump to corret index
    word tileRow = ((localY / 8) % 32) * 32;  // vertical tiles index
    // plus all three index and get the actual tileAddress
    word tileAddress = backgroundMem + tileRow + (tileCol % 32);
    int8_t rawTile{};
    if (m_isGBC) {
      rawTile = (int8_t)(m_vram[0][tileAddress - 0x8000]);
    } else {
      rawTile = (int8_t)ReadMemory(tileAddress);
    }

    // for GBC
    byte tileAttr = 0;
    byte tilePalette = 0;
    bool tileVramBank = false;
    bool tileHFlip = false;
    bool tileVFlip = false;
    bool tilePrio = false;
    if (m_isGBC) {
      tileAttr = m_vram[1][tileAddress - 0x8000];
      tilePalette = tileAttr & 0x7;
      tileVramBank = (tileAttr >> 3) & 0x1;
      tileHFlip = (tileAttr >> 5) & 0x1;
      tileVFlip = (tileAttr >> 6) & 0x1;
      tilePrio = (tileAttr >> 7) & 1;
    }
    word tileLocation;
    if (unsig) {
      tileLocation = 0x8000 + ((byte)rawTile * 16);
    } else {
      // make the signed_word positive
      // for example;
      // signed_byte : -128 to 127
      //  -128 means the starting address 0;
      // so we get the actual address from 0x8800
      tileLocation = 0x9000 + (rawTile * 16);
    }

    // find the tile index of the current scanline to get the tileData
    byte tileLine = localY % 8;
    if (m_isGBC && tileVFlip) {
      tileLine = 7 - tileLine;
    }
    tileLine *= 2;  // as two byte are taken for color bit in memory
    byte data1, data2;
    if (m_isGBC) {
      data1 = m_vram[tileVramBank ? 1 : 0][tileLocation + tileLine - 0x8000];
      data2 =
          m_vram[tileVramBank ? 1 : 0][tileLocation + tileLine + 1 - 0x8000];
    } else {
      data1 = ReadMemory(tileLocation + tileLine);
      data2 = ReadMemory(tileLocation + tileLine + 1);
    }
    // pixel 0 correspond to data1 & data2 's bit 7
    // pixel 1 bit 6 ...
    byte colorBit = xPos % 8;
    if (!(m_isGBC && tileHFlip)) {
      colorBit = 7 - colorBit;
    }
    int colorNum = ((data2 >> colorBit) & 1) << 1;
    colorNum |= (data1 >> colorBit) & 1;
    int red = 0;
    int green = 0;
    int blue = 0;

    if (m_isGBC) {
      GBCcolor color = ReadColorGBC(colorNum, m_BGPalette, tilePalette);
      red = color.r;
      green = color.g;
      blue = color.b;
    } else {
      // get actual color  based on the colorNum and colorPalette
      byte color = ReadColor(colorNum, 0xFF47);
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
        case BLACK:
          red = 0x00;
          green = 0x00;
          blue = 0x00;
          break;
      }
    }

    // determine whether fall within the display region
    if ((scanline < 0 || scanline > 143) ||
        (pixel < 0 || pixel > 159)) {  // if not fall within display region
      continue;
    }
    m_bgIndex[pixel] = colorNum;
    m_bgPrio[pixel] = (m_isGBC && tilePrio && colorNum != 0);
    m_screenData[scanline][pixel][0] = red;
    m_screenData[scanline][pixel][1] = green;
    m_screenData[scanline][pixel][2] = blue;
    m_screenData[scanline][pixel][3] = 0xFF;
  }
}

// Color Code Map to Color Palette
// 00:White      bit 1-0
// 01:Light Grey bit 3-2
// 10:Dark Grey  bit 5-4
// 11:Black bit 7-6
COLOUR GameBoy::ReadColor(int colorNum, word address) {
  byte colorPalette = ReadMemory(address);
  byte color = (colorPalette >> (colorNum * 2)) & 0x03;
  switch (color) {
    case 0: return WHITE;
    case 1: return LIGHT_GRAY;
    case 2: return DARK_GRAY;
    case 3: return BLACK;
  }
  return WHITE;
}

GBCcolor GameBoy::ReadColorGBC(int colorNum, byte palette[], byte paletteIdx) {
  // each color have 5 bit
  //  each palette has 4 colors and each color is 2 bytes (15bytes +1unsued)
  byte offset = (paletteIdx * 8) + (colorNum * 2);
  byte lo = palette[offset];
  byte hi = palette[offset + 1];
  // rgb555 means each is 5 bit(naming convension lol!!!!)
  word rgb555 = (hi << 8) | lo;
  byte r5 = rgb555 & 0x1F;          // first 5 bit
  byte g5 = (rgb555 >> 5) & 0x1F;   // second 5 bit
  byte b5 = (rgb555 >> 10) & 0x1F;  // third 5 bit
  GBCcolor result{};
  // screenData only expects the 8-bit value so
  // need to upscale it
  result.r = (r5 * 255) / 31;
  result.g = (g5 * 255) / 31;
  result.b = (b5 * 255) / 31;
  return result;
}

// Spirte RAM region:0x8000-0x8FFF
// Sprite Attri region:0xFE00-0xFE9F
void GameBoy::RenderSprites() {
  bool use8x16 = ((ReadMemory(0xFF40) & 4) !=
                  0);  // this bit 4 in control lcd register tells
                       // whether the sprite is 8x8 or 8x16
  byte scanline = ReadMemory(0xFF44);
  // collect up to 10 sprites on this scanline, in OAM order.
  // scanline can only update 10 objects per scanline
  int selected[10];
  int numSelected = 0;
  for (int i = 0; i < 40 && numSelected < 10; i++) {
    // Each Obj have 4 bytes so we jump by i*4
    int index = i * 4;
    // 0-byte = YPos(offset=16)
    // 1-byte =Xpos(offset=8)
    // 2-byte = Tile index
    // 3-byte  = Attribute
    int yPos = (int)ReadMemory(0xFE00 + index) - 16;
    int xPos = (int)ReadMemory(0xFE00 + index + 1) - 8;
    int ySize = use8x16 ? 16 : 8;
    // the scanline is the draw range
    if (scanline >= yPos && scanline < yPos + ySize) {
      selected[numSelected++] = index;
    }
  }
  // split into "behind BG" (bit 7 set) and "front" (bit 7 clear) groups
  int behind[10], front[10];
  int nBehind = 0, nFront = 0;
  for (int i = 0; i < numSelected; i++) {
    byte attrs = ReadMemory(0xFE00 + selected[i] + 3);
    if (attrs & 0x80) {  // bit 7 priority
      behind[nBehind++] = selected[i];
    } else {
      front[nFront++] = selected[i];
    }
  }

  // DMG: sort each group by X ascending (stable -> lower OAM wins ties)
  if (!m_isGBC) {
    int* groups[2] = {behind, front};
    int counts[2] = {nBehind, nFront};
    for (int g = 0; g < 2; g++) {
      for (int a = 0; a < counts[g]; a++) {
        for (int b = a + 1; b < counts[g]; b++) {
          int xa = (int)ReadMemory(0xFE00 + groups[g][a] + 1);
          int xb = (int)ReadMemory(0xFE00 + groups[g][b] + 1);
          if (xa > xb) {
            int tmp = groups[g][a];
            groups[g][a] = groups[g][b];
            groups[g][b] = tmp;
          }
        }
      }
    }
  }

  // draw behind group first, then front group
  for (int s = nBehind - 1; s >= 0; s--)
    DrawSpritePixels(behind[s], use8x16);
  for (int s = nFront - 1; s >= 0; s--)
    DrawSpritePixels(front[s], use8x16);
}

void GameBoy::DrawSpritePixels(int index, bool use8x16) {
  int yPos = (int)(ReadMemory(0xFE00 + index) - 16);
  int xPos = (int)(ReadMemory(0xFE00 + index + 1) - 8);
  byte tileLocation = ReadMemory(0xFE00 + index + 2);
  byte attributes = ReadMemory(0xFE00 + index + 3);
  bool flipY = (attributes & 64) != 0;
  bool flipX = (attributes & 32) != 0;
  byte spritePalette = attributes & 0x7;
  bool spritevramBank = (attributes >> 3) & 0x01;
  byte scanline = ReadMemory(0xFF44);
  byte line = scanline - yPos;
  if (flipY) {
    line = (byte)((use8x16 ? 16 : 8) - 1 - line);
  }
  line *= 2;
  if (use8x16) {
    tileLocation &= 0xFE;
  }
  word tileAddress = 0x8000 + (tileLocation * 16) + line;
  byte data1, data2;
  if (m_isGBC) {
    // PPU always fetches from the bank selected by the OAM attribute bit 3,
    // regardless of the CPU VBK register (0xFF4F).
    data1 = m_vram[spritevramBank][tileAddress - 0x8000];
    data2 = m_vram[spritevramBank][tileAddress + 1 - 0x8000];
  } else {
    data1 = ReadMemory(tileAddress);
    data2 = ReadMemory(tileAddress + 1);
  }
  for (int pixelBit = 7; pixelBit >= 0; pixelBit--) {
    int colorBit = pixelBit;
    if (flipX) {
      colorBit -= 7;
      colorBit *= -1;
    }
    int colorNum = ((data2 >> colorBit) & 1) << 1;
    colorNum |= (data1 >> colorBit) & 1;
    int red = 0, green = 0, blue = 0;
    if (m_isGBC) {
      if (colorNum == 0)
        continue;
      GBCcolor color = ReadColorGBC(colorNum, m_OBJPalette, spritePalette);
      red = color.r;
      green = color.g;
      blue = color.b;
    } else {
      word colorAddress = (attributes & 16) != 0 ? 0xFF49 : 0xFF48;
      COLOUR color = ReadColor(colorNum, colorAddress);
      if (colorNum == 0) {
        continue;
      }
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
        case BLACK:
          red = 0x00;
          green = 0x00;
          blue = 0x00;
          break;
      }
    }
    int xPix = 7 - pixelBit;
    int pixel = xPix + xPos;
    if (scanline > 143 || pixel < 0 || pixel > 159)
      continue;
    bool masterPriority = m_isGBC ? ((ReadMemory(0xFF40) & 0x01) != 0) : true;
    if ((masterPriority && m_bgIndex[pixel] != 0) &&
        (m_bgPrio[pixel] || ((attributes >> 7) & 1)))
      continue;
    m_screenData[scanline][pixel][0] = red;
    m_screenData[scanline][pixel][1] = green;
    m_screenData[scanline][pixel][2] = blue;
    m_screenData[scanline][pixel][3] = 0xFF;
  }
}
void GameBoy::ScreenReset() {
  for (int x = 0; x < 144; x++) {
    for (int y = 0; y < 160; y++) {
      m_screenData[x][y][0] = 255;
      m_screenData[x][y][1] = 255;
      m_screenData[x][y][2] = 255;
    }
  }
}
