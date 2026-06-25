#include "./GameBoy.hpp"
#include <cstdio>
#include <cstring>

// Timer address
#define TIMA 0xFF05
#define TMA 0xFF06
#define TMC 0xFF07

// The consturctor will initialize and then set the required state of the emulator as if the real
// game has started
// gameboy doesn't have an isolate stack that's why it is allocated at the near end of higher byte
// after the interept slot 0xFFFF
GameBoy::GameBoy()
    :

      m_programCounter(0x100), m_RegisterAF{.reg = 0x01B0}, m_RegisterBC{.reg = 0x0013},
      m_RegisterDE{.reg = 0x00D8}, m_RegisterHL{.reg = 0x014D}, current_romBank(1),
      current_ramBank(0), m_MBU1(false), m_MBU2(false), m_enableRAM(false) {
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
            SetClockFeq();          // set new one
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
        if (m_TimerCounter <= 0) {         // when finished
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
    case 0:                    // 00
        m_TimerCounter = 1024; // feq 4096
        break;
    case 1:                  // 01
        m_TimerCounter = 16; // feq 262,144
        break;
    case 2:                  // 10
        m_TimerCounter = 64; // feq 65536
        break;
    case 3:                   // 11
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
        byte enabled = ReadMemory(0xFFFF); // this is the address for interrupt enabled
        if (req > 0) {
            for (int i = 0; i < 5; i++) {            // exactly 4 bit to check
                if ((req & (1 << i)) == 1) {         // check every bit in req
                    if ((enabled & (1 << i)) == 1) { // check every bit in enalbed
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
bool GameBoy::LCD_enabled() { return ((ReadMemory(0xFF40) & 128) == 0) ? false : true; }

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
        RequestInterrupt(1);               // LCD interrupt
    }
    if (currentLine == ReadMemory(0xFF45)) { // coincidence
        status |= 4;                         // turn on the 2th bit
        if ((status & 64) != 0) {            // if interrupt enabled
            RequestInterrupt(1);
        }
    } else {
        status &= ~4; // turn off  bit 2
    }
    WriteMemory(0xFF41, status);
}

// GameBoy doesn't allow the sprite ram(OAM) to update and delete when it is drawing. Only allow
// during v-blank, so to update during that v-blank period it is not enough time to update whole
// in that scenario, devs use dma
// DMA destination address(0xFE00-0xFE9F) which is exacly 0xA0 byte
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
// empty data to draw but here, we are not doing that fifo implementation for ppu
// and we just draw whatever has at 0
void GameBoy::RenderTiles() {
    word backgroundMem = 0;
    word tileData = 0;
    byte status = ReadMemory(0xFF40);
    bool unsig = false;
    bool usingWindow = false;
    byte scrollY = ReadMemory(0xFF42);
    byte scrollX = ReadMemory(0xFF43);
    byte windowY = ReadMemory(0xFF4A);
    byte windowX = ReadMemory(0xFF4B) - 7; // in order for the ppu to prefetch the data

    // check the window bit
    if ((status & 32) != 0) {                // check bit 5
        if (windowY <= ReadMemory(0xFF44)) { // only if y is less than or equal to scanline
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
        // we have to reset the yPos  as we need to get the correct index for tile data
        yPos = ReadMemory(0xFF44) - windowY;
    }
    // each tile is 8x8
    // so to get the tileRow index(current scanline pixel) we have to divide by  8 to yPos and
    // multiply 32 to jump to corret index
    word tileRow = (((byte)(yPos / 8)) * 32); // vertical tiles index
    // draw the horizontal pixel
    for (int pixel = 0; pixel < 160; pixel++) {
        byte xPos = scrollX + pixel;
        if (usingWindow) {
            if (pixel >= windowX) {     // at drawing window pixel
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
    bool use8x16 =
        ((ReadMemory(0xFF40) & 4) != 0)
            ? true
            : false; // this bit 4 in control lcd register tells whether the sprite is 8x8 or 8x16
    bool flipY = false;
    bool flipX = false;
    // each frame can render 40 sprite
    for (int sprite = 0; sprite < 40; sprite++) {
        // each sprite takes 4 bytes
        byte index = sprite * 4;
        // each of this is 1 byte = total=4
        byte yPos = ReadMemory(0xFE00 + index) - 16;
        byte xPos = ReadMemory(0xFE00 + index + 1) - 8;
        byte tileLocation = ReadMemory(0xFE00 + index + 2); // this one is for sprite pattern number
        byte attributes =
            ReadMemory(0xFE00 + index + 3); // this attri tells about the sprite
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
        if ((scanline >= yPos) && (scanline <= yPos + ySize)) { // within the y draw range
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
            // So the reason i didn't write for 8x16 specific is that this match automatically
            // handle that too
            // Row0: .......
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
                word colorAddress =
                    ((attributes & 16) != 0) ? 0xFF49 : 0xFF48; // check the bit 4 of attributes
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

                // I wrote the pixle to be in reverse but when drawing has to be from 0 to 7 so
                // for example ; if pixelbit is 7 that means the xPix= 0;
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
    byte opcode = ReadMemory(m_programCounter);
    m_programCounter++;
    res = ExcuteOpcode(opcode);
}

int GameBoy::ExcuteOpcode(byte opcode) {
    switch (opcode) {
    case 0x06:
        CPU_8bit_Load(m_RegisterBC.hi);
        return 8;
        break;
    case 0x80:
        CPU_8bit_ADD(m_RegisterAF.hi, m_RegisterBC.hi, false, false);
        return 4;
    case 0x90:
        CPU_8bit_SUB(m_RegisterAF.hi, m_RegisterBC.hi, false, false);
        return 4;
    case 0xA7:
        CPU_8bit_AND(m_RegisterAF.hi, m_RegisterAF.hi, false);
        return 4;
    case 0xAF:
        CPU_8bit_XOR(m_RegisterAF.hi, m_RegisterAF.hi, false);
        return 4;
    case 0xB7:
        CPU_8bit_OR(m_RegisterAF.hi, m_RegisterAF.hi, false);
        return 4;
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
// Add n to A
// n can be register or value
void GameBoy::CPU_8bit_ADD(byte &reg, byte toAdd, bool useImmediate, bool addCarry) {
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
    // the reason we check before+adding is because if we chekc the reg it will already be in
    // overflow state
    if (before + adding > 0xFF) { // check full carry
        m_RegisterAF.hi |= 1 << FLAG_C;
    }
}

void GameBoy::CPU_8bit_SUB(byte &reg, byte toSub, bool useImmediate, bool borrowCarry) {
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

};
