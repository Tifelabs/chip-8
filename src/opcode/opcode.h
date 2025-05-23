#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>


// System Constants
#define MEMORY_SIZE 4096
#define DISPLAY_WIDTH 64
#define DISPLAY_HEIGHT 32
#define GRAPHICS_SIZE (DISPLAY_WIDTH * DISPLAY_HEIGHT)
#define PROGRAM_START 0x200
#define FONTSET_START 0x50
#define STACK_SIZE 16
#define NUM_REGISTERS 16
#define NUM_KEYS 16



// System State
typedef struct {
    uint8_t memory[MEMORY_SIZE];        // Memory
    uint8_t display[GRAPHICS_SIZE];     // Display buffer
    uint8_t V[NUM_REGISTERS];           // CPU registers V0-VF
    uint16_t I;                         // Index register
    uint16_t pc;                        // Program counter
    uint16_t stack[STACK_SIZE];         // Call stack
    uint8_t sp;                         // Stack pointer
    uint8_t delay_timer;                // Delay timer
    uint8_t sound_timer;                // Sound timer
    uint8_t keys[NUM_KEYS];             // Key states
    int draw_flag;                      // Screen needs redraw
    int running;                        // Emulator running flag
} chip8_t;

// Fetch instruction from memory
uint16_t chip8_fetch(chip8_t *chip8) {
    if (chip8->pc >= MEMORY_SIZE - 1) {
        fprintf(stderr, "Error: PC out of bounds (0x%04X)\n", chip8->pc);
        chip8->running = 0;
        return 0;
    }
    
    uint16_t opcode = (chip8->memory[chip8->pc] << 8) | chip8->memory[chip8->pc + 1];
    chip8->pc += 2;
    return opcode;
}


// Execute instruction
void chip8_execute(chip8_t *chip8, uint16_t opcode) {
    uint8_t x = (opcode & 0x0F00) >> 8;
    uint8_t y = (opcode & 0x00F0) >> 4;
    uint8_t n = opcode & 0x000F;
    uint8_t nn = opcode & 0x00FF;
    uint16_t nnn = opcode & 0x0FFF;
    
    switch (opcode & 0xF000) {
        case 0x0000:
            switch (opcode) {
                case 0x00E0: // CLS - Clear screen
                    memset(chip8->display, 0, GRAPHICS_SIZE);
                    chip8->draw_flag = 1;
                    break;
                    
                case 0x00EE: // RET - Return from subroutine
                    if (chip8->sp == 0) {
                        fprintf(stderr, "Error: Stack underflow\n");
                        chip8->running = 0;
                        return;
                    }
                    chip8->sp--;
                    chip8->pc = chip8->stack[chip8->sp];
                    break;
                    
                default:
                    printf("Unknown 0x0 opcode: 0x%04X\n", opcode);
                    break;
            }
            break;
            
        case 0x1000: // JP nnn - Jump to address
            chip8->pc = nnn;
            break;
            
        case 0x2000: // CALL nnn - Call subroutine
            if (chip8->sp >= STACK_SIZE) {
                fprintf(stderr, "Error: Stack overflow\n");
                chip8->running = 0;
                return;
            }
            chip8->stack[chip8->sp] = chip8->pc;
            chip8->sp++;
            chip8->pc = nnn;
            break;
            
        case 0x3000: // SE Vx, nn - Skip if Vx == nn
            if (chip8->V[x] == nn) chip8->pc += 2;
            break;
            
        case 0x4000: // SNE Vx, nn - Skip if Vx != nn
            if (chip8->V[x] != nn) chip8->pc += 2;
            break;
            
        case 0x5000: // SE Vx, Vy - Skip if Vx == Vy
            if (chip8->V[x] == chip8->V[y]) chip8->pc += 2;
            break;
            
        case 0x6000: // LD Vx, nn - Set Vx = nn
            chip8->V[x] = nn;
            break;
            
        case 0x7000: // ADD Vx, nn - Add nn to Vx
            chip8->V[x] += nn;
            break;
            
        case 0x8000: // Arithmetic operations
            switch (n) {
                case 0x0: // LD Vx, Vy
                    chip8->V[x] = chip8->V[y];
                    break;
                case 0x1: // OR Vx, Vy
                    chip8->V[x] |= chip8->V[y];
                    break;
                case 0x2: // AND Vx, Vy
                    chip8->V[x] &= chip8->V[y];
                    break;
                case 0x3: // XOR Vx, Vy
                    chip8->V[x] ^= chip8->V[y];
                    break;
                case 0x4: // ADD Vx, Vy (with carry)
                    {
                        uint16_t sum = chip8->V[x] + chip8->V[y];
                        chip8->V[0xF] = (sum > 255) ? 1 : 0;
                        chip8->V[x] = sum & 0xFF;
                    }
                    break;
                case 0x5: // SUB Vx, Vy
                    chip8->V[0xF] = (chip8->V[x] > chip8->V[y]) ? 1 : 0;
                    chip8->V[x] -= chip8->V[y];
                    break;
                case 0x6: // SHR Vx
                    chip8->V[0xF] = chip8->V[x] & 0x1;
                    chip8->V[x] >>= 1;
                    break;
                case 0x7: // SUBN Vx, Vy
                    chip8->V[0xF] = (chip8->V[y] > chip8->V[x]) ? 1 : 0;
                    chip8->V[x] = chip8->V[y] - chip8->V[x];
                    break;
                case 0xE: // SHL Vx
                    chip8->V[0xF] = (chip8->V[x] & 0x80) >> 7;
                    chip8->V[x] <<= 1;
                    break;
                default:
                    printf("Unknown 0x8 opcode: 0x%04X\n", opcode);
                    break;
            }
            break;
            
        case 0x9000: // SNE Vx, Vy - Skip if Vx != Vy
            if (chip8->V[x] != chip8->V[y]) chip8->pc += 2;
            break;
            
        case 0xA000: // LD I, nnn - Set I = nnn
            chip8->I = nnn;
            break;
            
        case 0xB000: // JP V0, nnn - Jump to nnn + V0
            chip8->pc = nnn + chip8->V[0];
            break;
            
        case 0xC000: // RND Vx, nn - Set Vx = random & nn
            chip8->V[x] = (rand() % 256) & nn;
            break;
            
        case 0xD000: // DRW Vx, Vy, n - Draw sprite
            {
                uint8_t x_pos = chip8->V[x] % DISPLAY_WIDTH;
                uint8_t y_pos = chip8->V[y] % DISPLAY_HEIGHT;
                chip8->V[0xF] = 0;
                
                for (int row = 0; row < n; row++) {
                    if (chip8->I + row >= MEMORY_SIZE) break;
                    
                    uint8_t sprite_byte = chip8->memory[chip8->I + row];
                    for (int col = 0; col < 8; col++) {
                        if (sprite_byte & (0x80 >> col)) {
                            int pixel_x = (x_pos + col) % DISPLAY_WIDTH;
                            int pixel_y = (y_pos + row) % DISPLAY_HEIGHT;
                            int idx = pixel_y * DISPLAY_WIDTH + pixel_x;
                            
                            if (chip8->display[idx] == 1) {
                                chip8->V[0xF] = 1;
                            }
                            chip8->display[idx] ^= 1;
                        }
                    }
                }
                chip8->draw_flag = 1;
            }
            break;
            
        case 0xE000: // Key operations
            switch (nn) {
                case 0x9E: // SKP Vx - Skip if key Vx is pressed
                    if (chip8->keys[chip8->V[x] & 0xF]) chip8->pc += 2;
                    break;
                case 0xA1: // SKNP Vx - Skip if key Vx is not pressed
                    if (!chip8->keys[chip8->V[x] & 0xF]) chip8->pc += 2;
                    break;
                default:
                    printf("Unknown 0xE opcode: 0x%04X\n", opcode);
                    break;
            }
            break;
            
        case 0xF000: // Misc operations
            switch (nn) {
                case 0x07: // LD Vx, DT - Set Vx = delay timer
                    chip8->V[x] = chip8->delay_timer;
                    break;
                case 0x0A: // LD Vx, K - Wait for key press
                    {
                        int key_pressed = -1;
                        for (int i = 0; i < NUM_KEYS; i++) {
                            if (chip8->keys[i]) {
                                key_pressed = i;
                                break;
                            }
                        }
                        if (key_pressed >= 0) {
                            chip8->V[x] = key_pressed;
                        } else {
                            chip8->pc -= 2; // Wait for key
                        }
                    }
                    break;
                case 0x15: // LD DT, Vx - Set delay timer = Vx
                    chip8->delay_timer = chip8->V[x];
                    break;
                case 0x18: // LD ST, Vx - Set sound timer = Vx
                    chip8->sound_timer = chip8->V[x];
                    break;
                case 0x1E: // ADD I, Vx - Add Vx to I
                    chip8->I += chip8->V[x];
                    break;
                case 0x29: // LD F, Vx - Set I = sprite location for digit Vx
                    chip8->I = FONTSET_START + (chip8->V[x] & 0xF) * 5;
                    break;
                case 0x33: // LD B, Vx - Store BCD of Vx in I, I+1, I+2
                    if (chip8->I + 2 < MEMORY_SIZE) {
                        chip8->memory[chip8->I] = chip8->V[x] / 100;
                        chip8->memory[chip8->I + 1] = (chip8->V[x] / 10) % 10;
                        chip8->memory[chip8->I + 2] = chip8->V[x] % 10;
                    }
                    break;
                case 0x55: // LD [I], Vx - Store V0-Vx in memory starting at I
                    for (int i = 0; i <= x && (chip8->I + i) < MEMORY_SIZE; i++) {
                        chip8->memory[chip8->I + i] = chip8->V[i];
                    }
                    break;
                case 0x65: // LD Vx, [I] - Load V0-Vx from memory starting at I
                    for (int i = 0; i <= x && (chip8->I + i) < MEMORY_SIZE; i++) {
                        chip8->V[i] = chip8->memory[chip8->I + i];
                    }
                    break;
                default:
                    printf("Unknown 0xF opcode: 0x%04X\n", opcode);
                    break;
            }
            break;
            
        default:
            printf("Unknown opcode: 0x%04X\n", opcode);
            break;
    }
}

// Execute one CPU cycle
void chip8_cycle(chip8_t *chip8) {
    if (!chip8->running) return;
    
    uint16_t opcode = chip8_fetch(chip8);
    chip8_execute(chip8, opcode);
}