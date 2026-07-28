import Register16 from "./types";
class GameBoy {

    private m_CartridgeMemory: Uint8Array;
    private m_rom: Uint8Array;
    m_programCounter: Uint16Array;
    private m_MBC1: boolean;
    private m_MBC2: boolean;
    private m_MBC3: boolean;
    private m_hiRomEnable: boolean; //for mbc1
    private m_RTC: number;
    private m_RTCregEnable: boolean; //for mbc3's RTC
    private m_ramEnable: boolean;
    private m_currentRomBank: number;
    private m_currentRamBank: number;
    private m_ramBanks: Uint16Array;
    constructor() {
        this.m_CartridgeMemory = new Uint8Array(2097152); //0x200000
        this.m_programCounter = new Uint16Array(1);
        this.m_programCounter[0] = 0x200;
        this.m_MBC1 = false;
        this.m_MBC2 = false;
        this.m_MBC3 = false;
        this.m_RTCregEnable = false;
        this.m_hiRomEnable = false;
        this.m_ramEnable = false;
        this.m_ramBanks = new Uint16Array(32768); //0x8000;
        this.m_currentRamBank = 0;
        this.m_rom = new Uint8Array(65536);//0x10000;
        this.m_currentRomBank = 1;
        this.m_rom[0xFF05] = 0x00;  // TIMA
        this.m_rom[0xFF06] = 0x00;  // TMA
        this.m_rom[0xFF07] = 0x00;  // TAC
        this.m_rom[0xFF10] = 0x80;  // NR10
        this.m_rom[0xFF11] = 0xBF;  // NR11
        this.m_rom[0xFF12] = 0xF3;  // NR12
        this.m_rom[0xFF14] = 0xBF;  // NR14
        this.m_rom[0xFF16] = 0x3F;  // NR21
        this.m_rom[0xFF17] = 0x00;  // NR22
        this.m_rom[0xFF19] = 0xBF;  // NE24
        this.m_rom[0xFF1A] = 0x7F;  // NR30
        this.m_rom[0xFF1B] = 0xFF;  // NR31
        this.m_rom[0xFF1C] = 0x9F;  // NR32
        this.m_rom[0xFF1E] = 0xBF;  // NR33
        this.m_rom[0xFF20] = 0xFF;  // NR41
        this.m_rom[0xFF21] = 0x00;  // NR42
        this.m_rom[0xFF22] = 0x00;  // NR43
        this.m_rom[0xFF23] = 0xBF;  // NR30
        this.m_rom[0xFF24] = 0x77;  // NR50
        this.m_rom[0xFF25] = 0xF3;  // NR51
        this.m_rom[0xFF26] = 0xF1;  // NR52
        this.m_rom[0xFF40] = 0x91;  // LCDC
        this.m_rom[0xFF42] = 0x00;  // SCY
        this.m_rom[0xFF43] = 0x00;  // SCX
        this.m_rom[0xFF45] = 0x00;  // LYC
        this.m_rom[0xFF47] = 0xE4;  // BGP
        this.m_rom[0xFF48] = 0xFF;  // OBP0
        this.m_rom[0xFF49] = 0xFF;  // OBP1
        this.m_rom[0xFF4A] = 0x00;  // WY
        this.m_rom[0xFF4B] = 0x00;  // WX
        this.m_rom[0xFFFF] = 0x00;
    }

    public update(): number {
        //  int cycles = NextOpCodeExcute();
        // UpdateTimers(cycles);
        // UpdateGraphics(cycles);
        // DoInterrupts();
        // return cycles;
        return 0;
    }



    //reminder:::
    //when writing to the region below 0x7FFF
    //it should interpret as command and not data

    private writeMemory(address: number, data: number): void {
        //here we handle that command case
        if (address < 0x8000) {
            this.handleBanking(address, data);
        }

        //wirte into ram region
        else if ((address >= 0xA000) && (address < 0xC000)) {
            if (this.m_ramEnable) {
                const word_addr: number = address - 0xA000;
                this.m_ramBanks[word_addr + (this.m_currentRamBank * 0x2000)] = data;
            }
        }
        //writing to echo ram also write to RAM
        else if ((address >= 0xE000) && (address < 0xFE00)) {
            this.m_rom[address] = data;
            this.writeMemory(address - 0x2000, data);
        }
        //this is restricted and empty but useful for i/o
        else if ((address >= 0xFEA0) && (address < 0xFEFF)) {
        }
        else {
            this.m_rom[address] = data;
        }
    }

    //mbc1 is weird it look for the mode in the 0x6000-0x8000
    //to see if rom or ram
    private handleBanking(address: number, data: number): void {
        //ram enabling
        if (address < 0x2000) {
            //have to check this only when the switch rom is enabled
            //if not  then using the default region and no mbcs are using so simple write
            if (this.m_MBC1 || this.m_MBC2 || this.m_MBC3) {
                this.doRamBankEnable(address, data);
            }
        }

        //for rom bank change
        if ((address >= 0x2000) && (address < 0x4000)) {
            if (this.m_MBC1 || this.m_MBC2 || this.m_MBC3) {
                this.doChangeLoRomBank(data);
            }

        }
        //mbc2 doesn't use this either
        if ((address >= 0x4000) && (address <= 0x5FFF)) {
            if (this.m_MBC1) {
                if (this.m_hiRomEnable) {
                    this.doChangeHiRomBank(data);
                } else {
                    this.doChangeRamBank(data);
                }
            } else if (this.m_MBC3) {
                this.doChangeRamOrRTC(data);
            }

        }
        //mbc2 doesn't use this
        if ((address >= 0x6000) && (address < 0x8000)) {
            if (this.m_MBC1) {
                this.handleModeSelect(data);
            } else if (this.m_MBC3) {
                this.handleRTCLatch(data);
            }
        }
    }


    private handleRTCLatch(data: number) {

    }

    //only 2-bit are set as new value
    private doChangeRamBank(data: number) {
        this.m_currentRamBank = data & 0x03;//only 2-bit are allowed
    }


    //for mbc3 it does two thing if the data is less than
    //3 (which is 0-1) bit ==11 in bit
    // it does set to currentRamBank and change the RTC to false
    // if not then enable it and then handle the rtc latch
    private doChangeRamOrRTC(data: number) {
        this.m_RTC = data;
        if (data <= 3) {
            this.m_currentRamBank = data;
            this.m_RTCregEnable = false;
        } else if (data >= 0x08 && data <= 0x0C) {
            this.m_RTCregEnable = true;
        }
    }


    //if the 0 bit is 0 means the rom is true and ram is default to 0
    private handleModeSelect(data: number) {
        const new_data: number = data & 0x1;
        this.m_hiRomEnable = (new_data === 0) ? true : false;
        if (this.m_hiRomEnable) {
            this.m_currentRamBank = 0;
        }
    }

    //check are all the same for mbc1,3,5 but for 2 have to extra check specific 8 bit 
    private doRamBankEnable(address: number, data: number): void {
        if (this.m_MBC2) {
            if ((address & 0x0100) !== 0) { //check for the 8 bit ,256= 0x0100
                return;
            }
        }
        const value = data & 0x0F;
        if (value === 0xA) {
            this.m_ramEnable = true;
            if (this.m_MBC3) {
                this.m_RTCregEnable = true;
            }
        }
    }

    //in this case, mbc2 operate diff too  and mbc3 does operate diff too
    private doChangeLoRomBank(data: number): void {
        //for MBC2 case
        //it takes the lower  4 bits and then  set it to current Rom Bank
        if (this.m_MBC2) {
            this.m_currentRomBank = data & 0xF;
            if (this.m_currentRomBank === 0) {
                this.m_currentRomBank++;
            }
            return;
        }
        //for MBC3 case
        //it takes the 7bits of data and set it to current rom Bank
        if (this.m_MBC3) {
            this.m_currentRomBank = data & 0x7F;
            if (this.m_currentRomBank === 0) {
                this.m_currentRomBank++;
            }
            return;
        }
        //for other MBCs
        //it takes the lower 5 bits and merge it with current Rom banks
        const low5: number = data & 31; //take out lower 5 bits
        this.m_currentRomBank &= 224;//turn of lower  5 bits
        this.m_currentRomBank |= low5;

        if (this.m_currentRomBank === 0) {
            this.m_currentRomBank++;
        }
    }

    //same thing as loRomBank but the key diff is that 
    //this take out 5 bit from romBank and then take out 3 bit from data  where as the loRomBank takes out the 5 bit from data and then  3 bit from romBank and merge well!!! both merge 
    private doChangeHiRomBank(data: number): void {
        this.m_currentRomBank &= 31;//take out  5 bit
        const new_data: number = (data & 0x03) << 5; // take out 3 bit
        this.m_currentRomBank |= new_data;
        if (this.m_currentRomBank === 0) {
            this.m_currentRomBank++;
        }
    }


    private readMemory(address: number): number {
        //from rom bank 0
        if (address < 0x4000) {
            return this.m_CartridgeMemory[address] ?? 0xFF;
        }
        //from switchable rom bank
        if ((address >= 0x4000) && (address <= 0x7FFF)) {
            const new_addr: number = address - 0x4000;
            return this.m_CartridgeMemory[new_addr + (this.m_currentRomBank * 0x4000)] ?? 0xFF;
        }

        //from ram bank
        if ((address >= 0xA000) && (address <= 0xBFFF)) {
            const new_addr: number = address - 0xA000;
            return this.m_ramBanks[new_addr + (this.m_currentRamBank * 0x2000)] ?? 0xFF;
        }

        return this.m_rom[address] ?? 0xFF;

    }



    public async readRom(path: string): Promise<void> {
        const file = Bun.file(path);
        try {
            this.m_CartridgeMemory = await file.bytes();
        } catch (e) {
            console.error(`Failed to read file:${e}`);
        }
        //read from 0x147 i can use readmemory but this is more good i guess
        const mbcType = this.m_CartridgeMemory[0x147];
        if (!mbcType) {
            console.error(`Failed to get the mbcType:${mbcType}`);
            return;
        }
        if (mbcType >= 1 && mbcType <= 3) {
            this.m_MBC1 = true;
        } else if (mbcType === 5 || mbcType === 6) {
            this.m_MBC2 = true;
        } else if (mbcType >= 0x0F && mbcType <= 0x13) {
            this.m_MBC3 = true;
        }
    }
}
