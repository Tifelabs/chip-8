#include <stdio.h>
#include <stdint.h>

#define WIDTH 64
#define HEIGHT 32

uint8_t screen[WIDTH * HEIGHT]; // 2048 bytes, one per pixel
uint8_t memory[4096];           // 4KB RAM
uint8_t V[16];                  // General purpose registers (V0-VF)
uint16_t PC = 0x200;            // Program Counter (starts at 0x200)
uint16_t I;                     // Index register
uint8_t delay_timer;            // Timers
uint8_t sound_timer;

uint8_t fontset[80] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

void load_fontset() {
    for (int i = 0; i < 80; i++) {
        memory[i] = fontset[i];
    }
}

void init_screen() {
    for (int i = 0; i < WIDTH * HEIGHT; i++) {
        screen[i] = 0;
    }
}

uint16_t fetch_opcode() {
    uint16_t opcode = memory[PC] << 8 | memory[PC + 1];
    PC += 2; // Next instruction
    return opcode;
}

void execute_opcode(uint16_t opcode) {
    if (opcode == 0x00E0) {
        for (int i = 0; i < WIDTH * HEIGHT; i++) {
            screen[i] = 0; // Clear all pixels
        }
        printf("Screen Cleared!\n");
    } else if (opcode == 0x00EE) {
        printf("Returned!\n"); // Placeholder—stack not implemented yet
    } else {
        printf("Unknown opcode: 0x%X\n", opcode);
    }
}

int main() {
    load_fontset();
    init_screen();

    // Test program: CLEA (00E0) then RET (00EE)
    memory[0x200] = 0x00;
    memory[0x201] = 0xE0;
    memory[0x202] = 0x00;
    memory[0x203] = 0xEE;

    // Simple CPU loop (run 2 instructions)
    for (int i = 0; i < 2; i++) {
        uint16_t opcode = fetch_opcode();
        printf("Fetched opcode: 0x%X at PC: 0x%X\n", opcode, PC - 2);
        execute_opcode(opcode);
    }

    printf("Final PC: 0x%X\n", PC);
    printf("Top-left pixel: %d\n", screen[0]);
    return 0;
}