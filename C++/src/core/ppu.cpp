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
  SetLCD_status();
  // HBlank DMA: one 16-byte chunk per HBlank while mode 0 is active
  if (m_hdmaActive && m_hdmaHBlankMode) {
    if ((ReadMemory(0xFF41) & 0x3) == 0 && ReadMemory(0xFF44) < 144 &&
        !m_hdmaLineDone) {
      DoHDMAChunk();
      m_hdmaLineDone = true;
    }
  }
  if (LCD_enabled()) {
    m_scalineCounter -= cycles;
  } else {
    return;
  }
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
  word targetAddr = address << 8;  // shift by right 8 to form 16bit
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
  if ((status & 1) != 0) {  // check bit 0
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
  word tileData = 0;
  byte status = ReadMemory(0xFF40);
  bool unsig = false;
  bool usingWindow = false;
  byte scrollY = ReadMemory(0xFF42);
  byte scrollX = ReadMemory(0xFF43);
  byte windowY = ReadMemory(0xFF4A);
  byte windowX =
      ReadMemory(0xFF4B) - 7;  // in order for the ppu to prefetch the data

  // check the window bit
  if ((status & 32) != 0) {  // check bit 5
    if (windowY <=
        ReadMemory(0xFF44)) {  // only if y is less than or equal to scanline
      usingWindow = true;
    }
  }
  // check the tile data select bit
  if ((status & 16) != 0) {  // check bit 4
    tileData = 0x8000;
    unsig = true;
  } else {
    tileData = 0x8800;  // this memory region use signed byte
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

  // draw the horizontal pixel
  for (int pixel = 0; pixel < 160; pixel++) {
    byte xPos = scrollX + pixel;
    byte localY = 0;
    if (usingWindow && pixel >= windowX) {  // at drawing window pixel
      xPos = pixel - windowX;               // reset
      localY = ReadMemory(0xFF44) - windowY;
      // whcih window memory region
      if ((status & 64) != 0) {  // check bit 6
        backgroundMem = 0x9C00;
      } else {
        backgroundMem = 0x9800;
      }
    } else {
      xPos = scrollX + pixel;
      localY = ReadMemory(0xFF44) + scrollY;
      if ((status & 8) != 0) {  // check bit 3
        backgroundMem = 0x9C00;
      } else {
        backgroundMem = 0x9800;
      }
    }

    // horizontal tile index
    word tileCol = xPos / 8;
    signed_word tileNum;
    // each tile is 8x8
    // so to get the tileRow index(current scanline pixel) we have to divide by
    // 8 to updated localY and multiply 32 to jump to corret index
    word tileRow = (((byte)(localY / 8)) * 32);  // vertical tiles index
    // plus all three index and get the actual tileAddress
    word tileAddress = backgroundMem + tileRow + tileCol;
    if (unsig) {
      //  unsig
      //  as the signed_word is 16 bit it can handle the unsigned_byte(8 bit)
      tileNum = (byte)ReadMemory(tileAddress);
    } else {
      // sig
      tileNum = (signed_byte)((byte)ReadMemory(tileAddress));
    }
    // for GBC
    byte tileAttr = 0;
    byte tilePalette = 0;
    bool tileVramBank = false;
    bool tileHFlip = false;
    bool tileVFlip = false;
    if (m_isGBC) {
      tileAttr = m_vram[1][tileAddress - 0x8000];
      tilePalette = tileAttr & 0x7;
      tileVramBank = (tileAttr >> 3) & 0x1;
      tileHFlip = (tileAttr >> 5) & 0x1;
      tileVFlip = (tileAttr >> 6) & 0x1;
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
    byte index = localY % 8;
    if (m_isGBC && tileVFlip) {
      index = 7 - index;
    }
    index *= 2;  // as two byte are taken for color bit in memory
    byte data1, data2;
    if (m_isGBC && tileVramBank) {
      data1 = m_vram[1][tileLocation + index - 0x8000];
      data2 = m_vram[1][tileLocation + index + 1 - 0x8000];
    } else {
      data1 = ReadMemory(tileLocation + index);
      data2 = ReadMemory(tileLocation + index + 1);
    }
    // pixel 0 correspond to data1 & data2 's bit 7
    // pixel 1 bit 6 ...
    byte colorBit = xPos % 8;
    if (m_isGBC && tileHFlip) {
    } else {
      colorBit -= 7;
      colorBit *= -1;
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
      }
    }
    // read the current pixel or lcd scanline coord
    byte finally = ReadMemory(0xFF44);
    // determine whether fall within the display region
    if ((finally < 0 || finally > 143) ||
        (pixel < 0 || pixel > 159)) {  // if not fall within display region
      continue;
    }

    m_screenData[finally][pixel][0] = red;
    m_screenData[finally][pixel][1] = green;
    m_screenData[finally][pixel][2] = blue;
    m_screenData[finally][pixel][3] = 0xFF;
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
    case 0: res = WHITE; break;
    case 1: res = LIGHT_GRAY; break;
    case 2: res = DARK_GRAY; break;
    case 3: res = BLACK; break;
  }

  return res;
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
  bool use8x16 = ((ReadMemory(0xFF40) & 4) != 0)
                     ? true
                     : false;  // this bit 4 in control lcd register tells
                               // whether the sprite is 8x8 or 8x16
  bool flipY = false;
  bool flipX = false;
  // each frame can render 40 sprite
  for (int sprite = 39; sprite >= 0; sprite--) {
    // each sprite takes 4 bytes
    byte index = sprite * 4;
    // each of this is 1 byte = total=4
    byte yPos = ReadMemory(0xFE00 + index) - 16;
    byte xPos = ReadMemory(0xFE00 + index + 1) - 8;
    byte tileLocation = ReadMemory(0xFE00 + index +
                                   2);  // this one is for sprite pattern number
    byte attributes = ReadMemory(
        0xFE00 + index + 3);  // this attri tells about the sprite
                              // Attribute bit
                              // bit 7 priority
                              // bit 6 y flip
                              // bit 5 x flip
                              // bit 4 palette number (0=0xFF48,1=0xFF49)
    flipY = ((attributes & 64) != 0) ? true : false;
    flipX = ((attributes & 32) != 0) ? true : false;
    // bit 7: priority        (same as DMG)
    // bit 6: y flip          (same as DMG)
    // bit 5: x flip          (same as DMG)
    // bit 4: DMG palette select (ignored on GBC)
    // bit 3: tile VRAM bank  (GBC only — which VRAM bank the tile pixel data
    // lives in) bit 0-2: GBC palette number (GBC only — which of 8 OBJ
    // palettes, replaces bit 4's binary choice) GBC specific fields
    byte spritePalette = attributes & 0x7;
    bool spritevramBank = (attributes >> 3) & 0x01;

    byte scanline = ReadMemory(0xFF44);
    int ySize = 8;
    if (use8x16) {
      ySize = 16;
    }
    if ((scanline >= yPos) &&
        (scanline < yPos + ySize)) {  // within the y draw range
      byte line = scanline - yPos;
      if (flipY) {
        // covert the 0 to last if the line is 0
        // so fliping is just reading from last rather than from start
        line -= ySize;
        line *= -1;
      }
      line *= 2;  // same as tiles
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
      byte data1, data2;
      if (m_isGBC && spritevramBank) {
        data1 = m_vram[1][tileAddress - 0x8000];
        data2 = m_vram[1][tileAddress + 1 - 0x8000];
      } else {
        data1 = ReadMemory(tileAddress);
        data2 = ReadMemory(tileAddress + 1);
      }

      // now start the horizontal pixel
      //  the reason backward is because of how the colorbit map to pixel bit
      //  same as tile colour bit
      for (int pixelbit = 7; pixelbit >= 0; pixelbit--) {
        int colorBit = pixelbit;
        if (flipX) {  // flip it my boysss!!!!
          colorBit -= 7;
          colorBit *= -1;
        }
        // same as tile pixel
        int colorNum = ((data2 >> colorBit) & 1) << 1;
        colorNum |= (data1 >> colorBit) & 1;

        int red = 0;
        int green = 0;
        int blue = 0;
        if (m_isGBC) {
          if (colorNum == 0) {
            continue;
          }
          GBCcolor color = ReadColorGBC(colorNum, m_OBJPalette, spritePalette);
          red = color.r;
          green = color.g;
          blue = color.b;
        } else {
          word colorAddress = ((attributes & 16) != 0)
                                  ? 0xFF49
                                  : 0xFF48;  // check the bit 4 of attributes
          COLOUR color = ReadColor(colorNum, colorAddress);
          if (color == 0) {
            // skip this current loop
            // why skip: the white is used for transparency
            continue;
          }

          // only handle the two Enum
          // Cuz other two are already handled by default duhhhhhhhhhh
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

        // hidden check
        if (((attributes >> 7) & 1) != 0) {
          if ((m_screenData[scanline][pixel][0] != 255) ||
              (m_screenData[scanline][pixel][1] != 255) ||
              (m_screenData[scanline][pixel][2] != 255)) {
            continue;
          }
        }
        m_screenData[scanline][pixel][0] = red;
        m_screenData[scanline][pixel][1] = green;
        m_screenData[scanline][pixel][2] = blue;
        m_screenData[scanline][pixel][3] = 0xFF;
      }
    }
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
