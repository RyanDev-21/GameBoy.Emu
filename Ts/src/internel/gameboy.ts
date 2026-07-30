class GameBoy {
    private m_CartridgeMemory: Uint8Array;
    private m_rom: Uint8Array;
    m_programCounter: Uint16Array;
    private m_MBC1: boolean;
    private m_MBC2: boolean;
    private m_MBC3: boolean;
    private m_hiRomEnable: boolean; //for mbc1
    private m_RTCregEnable: boolean; //for mbc3's RTC
    private m_RTCregs: Uint8Array;
    private m_RTCidx: number;
    private m_LatchRTC: Uint8Array;
    private m_RTCWriteState: number;
    private m_RTCAccumulator: number;
    private m_RTCTimeStamp: number;
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
        this.m_RTCWriteState = 0xFF;
        this.m_RTCAccumulator = 0; //for  seconds counter and stuff
        this.m_LatchRTC = new Uint8Array(5); //for snapshot
        this.m_RTCregs = new Uint8Array(5); //data 
        this.m_RTCidx = 0;
        this.m_RTCTimeStamp = Date.now();
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
                if (this.m_MBC3 && this.m_RTCregEnable) {
                    this.writeRTCReg(data);
                } else {
                    const word_addr: number = address - 0xA000;
                    this.m_ramBanks[word_addr + (this.m_currentRamBank * 0x2000)] = data;
                }
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

    //this is the update function for rtc and use in cpu cycle to update the rtc 
    //timer RTC[4]'s bit 6 indicate if it is halt or not 
    //if halt does nothing if not
    //then it has to tick per second 
    public updateRTC(secondsLapsed: number) {
        if ((this.m_RTCregs[4]! & 0x40) !== 0) {//if halt
            return;
        }
        this.m_RTCAccumulator += secondsLapsed; //this plus in 1/60 per second
        while (this.m_RTCAccumulator >= 1.0) {
            this.m_RTCAccumulator -= 1.0;
            this.tickOneSecond();
        }

    }

    //this function works like a water fall as the 
    //sec reach certain level then goes to min and stuff and so on
    //day counter has 9bit and the  reg 4 works as 8 bit and 5 works as a carry bit for  day counter
    private tickOneSecond(): void {
        //tick sec 
        this.m_RTCregs[0]!++;
        //check 60 secs up or not
        if (this.m_RTCregs[0]! <= 59) return;
        this.m_RTCregs[0] = 0;


        //tick minute
        this.m_RTCregs[1]! ++;
        if (this.m_RTCregs[1]! <= 59) return;
        this.m_RTCregs[1] = 0;

        //tick hour
        this.m_RTCregs[2]!++;
        if ((this.m_RTCregs[2]! <= 23)) return;
        this.m_RTCregs[2] = 0;

        //tick day
        this.m_RTCregs[3]!++;
        if (this.m_RTCregs[3]! <= 0xFF) return;
        this.m_RTCregs[3] = 0;

        //this is the most significant bit for day counter 
        //and bit0 represents that bit 
        //so for the first 256 days it turns into 1
        //and then it turns into 0 again when it reaches the 512
        const dayMSB = (this.m_RTCregs[4]! & 0x01) ^ 0x01;
        //this merge that updated bit to the value
        this.m_RTCregs[4] = (this.m_RTCregs[4]! & 0xFE) | dayMSB;
        if (dayMSB === 0) {//check if it is 512 overflow 
            this.m_RTCregs[4] |= 0x80;//turn on the overflow bit
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
            if (!this.m_ramEnable) return 0xFF;
            if (this.m_MBC3 && this.m_RTCregEnable) {
                return this.readRTCReg();
            } else {
                const new_addr: number = address - 0xA000;
                return this.m_ramBanks[new_addr + (this.m_currentRamBank * 0x2000)] ?? 0xFF;
            }
        }

        return this.m_rom[address] ?? 0xFF;

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
        //mbc2 also doesn't use this 
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


    //writing the consequetive  0x00 and 0x01 trigger the save state of the rtc to the latchrtc
    private handleRTCLatch(data: number) {
        if (this.m_RTCWriteState === 0x00 && data == 0x01) {
            this.m_LatchRTC.set(this.m_RTCregs);
            return;
        }
        this.m_RTCWriteState = data;
    }

    //writing into RTC reg is simple 
    //for index 0 to 3 just update the value
    //but for index 4 the last reg only use 7,6,0 bits
    private writeRTCReg(data: number) {
        const idx: number = this.m_RTCidx - 0x08;
        if (idx === 4) {
            this.m_RTCregs[4] = data & 0xC1;
            return;
        }
        this.m_RTCregs[idx] = data;
    }


    private readRTCReg(): number {
        const idx = this.m_RTCidx - 0x08;
        return this.m_RTCregs[idx] ?? 0;
    }


    //this fastforward function will recalcualte the time passed since save and reapply to the 
    //rtc registers
    public fastForwardRTC(elapsedSeconds: number): void {
        //halt or not 
        if ((this.m_RTCregs[4]! & 0x40) !== 0) {
            return;
        }

        //calc the total day by combining the msb bit from reg 4 and 8 bit from 3
        let totalDays = ((this.m_RTCregs[4]! & 0x01) << 8) | this.m_RTCregs[3]!;
        let totalSeconds = this.m_RTCregs[0]! +
            this.m_RTCregs[1]! * 60 +
            this.m_RTCregs[2]! * 3600 +
            totalDays * 86400;

        totalSeconds += Math.floor(elapsedSeconds);

        //if  over 512 which means that seonds have overflowed
        const overflow = totalSeconds >= 512 * 86400;
        totalSeconds %= 512 * 86400;//warp at the max limit of 9 bit

        this.m_RTCregs[0] = totalSeconds % 60;
        this.m_RTCregs[1] = Math.floor(totalSeconds / 60) % 60;
        this.m_RTCregs[2] = Math.floor(totalSeconds / 3600) % 24;
        const newDays = Math.floor(totalSeconds / 86400);
        this.m_RTCregs[3] = newDays & 0xFF;
        //move 8 bit to the right to get the msb
        let dh = (newDays >> 8) & 0x01;
        //for overflow
        if (overflow) {
            dh |= 0x80;
        }
        //for halt bit
        if ((this.m_RTCregs[4]! & 0x40) !== 0) {
            dh |= 0x40;
        }

        this.m_RTCregs[4] = dh;
        //copy the updated value into the latched rtc
        this.m_LatchRTC.set(this.m_RTCregs);
    }

    //only 2-bit are set as new value
    private doChangeRamBank(data: number) {
        this.m_currentRamBank = data & 0x03;
    }


    //for mbc3 it does two thing if the data is less than
    //3 (which is 0-1) bit ==11 in bit
    // it does set to currentRamBank and change the RTC to false
    // if not then enable it and then handle the rtc idx
    private doChangeRamOrRTC(data: number) {
        if (data <= 3) {
            this.m_currentRamBank = data;
            this.m_RTCregEnable = false;
        } else if (data >= 0x08 && data <= 0x0C) {
            this.m_RTCregEnable = true;
            this.m_RTCidx = data;
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
        this.m_ramEnable = value === 0xA0;

    }

    //in this case, mbc2 operate diff  and mbc3 does operate diff too
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

    public async readRom(path: string): Promise<void> {
        const file = Bun.file(path);
        try {
            this.m_CartridgeMemory = await file.bytes();
        } catch (e) {
            console.error(`Failed to read file:${e}`);
        }
        //read from 0x147 i can use readmemory but this is more good i guess
        const mbcType = this.m_CartridgeMemory[0x147];
        if (mbcType === undefined) {
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
