export default class Register16 {

    private buffer = new ArrayBuffer(2);
    private bytes = new Uint8Array(this.buffer);
    private word = new Uint16Array(this.buffer);
    constructor(lo: number = 0, hi: number = 0) {
        this.bytes[0] = lo;
        this.bytes[1] = hi;
    };


    get lo(): number {
        return this.bytes[0] ?? 0;
    }

    set lo(val: number) {
        this.bytes[0] = val;
    }
    get hi(): number {
        return this.bytes[1] ?? 0;
    }
    set hi(val: number) {
        this.bytes[1] = val;
    }


    get reg(): number {
        return this.word[0] ?? 0;
    }

    set reg(val: number) {
        this.word[0] = val;
    }
}

