#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#else
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/time.h>
#endif

// CHIP-8 System Constants
#define MEMORY_SIZE 4096
#define DISPLAY_WIDTH 64
#define DISPLAY_HEIGHT 32
#define GRAPHICS_SIZE (DISPLAY_WIDTH * DISPLAY_HEIGHT)
#define PROGRAM_START 0x200
#define FONTSET_START 0x50
#define STACK_SIZE 16
#define NUM_REGISTERS 16
#define NUM_KEYS 16

// Timing Constants
#define TARGET_FPS 60
#define CYCLES_PER_SECOND 700
#define CYCLES_PER_FRAME (CYCLES_PER_SECOND / TARGET_FPS)

// CHIP-8 System State
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

// Font set 
const uint8_t fontset[80] = {
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

// Key mapping (CHIP-8: 123C 456D 789E A0BF -> QWERTY: 1234 QWER ASDF ZXCV)
const char key_map[16] = {
    '1', '2', '3', '4',  // 1, 2, 3, C
    'q', 'w', 'e', 'r',  // 4, 5, 6, D
    'a', 's', 'd', 'f',  // 7, 8, 9, E
    'z', 'x', 'c', 'v'   // A, 0, B, F
};

// Function prototypes
void chip8_init(chip8_t *chip8);
int chip8_load_rom(chip8_t *chip8, const char *filename);
void chip8_cycle(chip8_t *chip8);
void chip8_update_timers(chip8_t *chip8);
void chip8_render(chip8_t *chip8);
void chip8_update_keys(chip8_t *chip8);
uint16_t chip8_fetch(chip8_t *chip8);
void chip8_execute(chip8_t *chip8, uint16_t opcode);

// Platform-specific functions
void platform_clear_screen(void);
int platform_kbhit(void);
char platform_getch(void);
void platform_sleep_ms(int ms);
long long platform_get_time_ms(void);

// Initialize CHIP-8 system
void chip8_init(chip8_t *chip8) {
    // Clear memory
    memset(chip8->memory, 0, MEMORY_SIZE);
    memset(chip8->display, 0, GRAPHICS_SIZE);
    memset(chip8->V, 0, NUM_REGISTERS);
    memset(chip8->stack, 0, STACK_SIZE * sizeof(uint16_t));
    memset(chip8->keys, 0, NUM_KEYS);
    
    // Initialize registers
    chip8->pc = PROGRAM_START;
    chip8->I = 0;
    chip8->sp = 0;
    chip8->delay_timer = 0;
    chip8->sound_timer = 0;
    chip8->draw_flag = 1;
    chip8->running = 1;
    
    // Load fontset into memory
    memcpy(&chip8->memory[FONTSET_START], fontset, 80);
    
    printf("CHIP-8 system initialized\n");
}

// Load ROM into memory
int chip8_load_rom(chip8_t *chip8, const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "Error: Could not open ROM file '%s'\n", filename);
        return 0;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);
    
    // Check if ROM fits in memory
    if (size > (MEMORY_SIZE - PROGRAM_START)) {
        fprintf(stderr, "Error: ROM too large. Max size: %d bytes\n", 
                MEMORY_SIZE - PROGRAM_START);
        fclose(file);
        return 0;
    }
    
    // Load ROM into memory
    size_t bytes_read = fread(&chip8->memory[PROGRAM_START], 1, size, file);
    fclose(file);
    
    if (bytes_read != (size_t)size) {
        fprintf(stderr, "Error: Could not read entire ROM file\n");
        return 0;
    }
    
    printf("Loaded ROM: %s (%ld bytes)\n", filename, size);
    return 1;
}

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

// Update timers
void chip8_update_timers(chip8_t *chip8) {
    if (chip8->delay_timer > 0) {
        chip8->delay_timer--;
    }
    
    if (chip8->sound_timer > 0) {
        chip8->sound_timer--;
        // TODO: Play beep sound
    }
}

// Render display
void chip8_render(chip8_t *chip8) {
    if (!chip8->draw_flag) return;
    
    platform_clear_screen();
    
    for (int y = 0; y < DISPLAY_HEIGHT; y++) {
        for (int x = 0; x < DISPLAY_WIDTH; x++) {
            int idx = y * DISPLAY_WIDTH + x;
            printf("%s", chip8->display[idx] ? "██" : "  ");
        }
        printf("\n");
    }
    printf("Press ESC to quit | Delay: %d | Sound: %d\n", 
           chip8->delay_timer, chip8->sound_timer);
    
    chip8->draw_flag = 0;
}

// Update key states
void chip8_update_keys(chip8_t *chip8) {
    // Reset all keys
    memset(chip8->keys, 0, NUM_KEYS);
    
    if (platform_kbhit()) {
        char key = platform_getch();
        
        // Check for quit
        if (key == 27) { // ESC key
            chip8->running = 0;
            return;
        }
        
        // Map key to CHIP-8 key
        for (int i = 0; i < NUM_KEYS; i++) {
            if (key == key_map[i]) {
                chip8->keys[i] = 1;
                break;
            }
        }
    }
}

// Platform-specific implementations
#ifdef _WIN32
void platform_clear_screen(void) {
    system("cls");
}

int platform_kbhit(void) {
    return _kbhit();
}

char platform_getch(void) {
    return _getch();
}

void platform_sleep_ms(int ms) {
    Sleep(ms);
}

long long platform_get_time_ms(void) {
    return GetTickCount64();
}

#else
void platform_clear_screen(void) {
    printf("\033[2J\033[1;1H");
}

int platform_kbhit(void) {
    struct termios oldt, newt;
    int ch;
    int oldf;
    
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
    
    ch = getchar();
    
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);
    
    if (ch != EOF) {
        ungetc(ch, stdin);
        return 1;
    }
    
    return 0;
}

char platform_getch(void) {
    return getchar();
}

void platform_sleep_ms(int ms) {
    usleep(ms * 1000);
}

long long platform_get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)(tv.tv_sec) * 1000 + (tv.tv_usec) / 1000;
}
#endif

// Main function
int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <rom_file>\n", argv[0]);
        return 1;
    }
    
    chip8_t chip8;
    srand((unsigned int)time(NULL));
    
    // Initialize system
    chip8_init(&chip8);
    
    // Load ROM
    if (!chip8_load_rom(&chip8, argv[1])) {
        return 1;
    }
    
    printf("CHIP-8 Emulator\n");
    printf("Keys: 1234 QWER ASDF ZXCV (mapped to CHIP-8 hex keypad)\n");
    printf("Press ESC to quit\n\n");
    
    // Main emulation loop
    long long last_time = platform_get_time_ms();
    int cycles_this_frame = 0;
    
    while (chip8.running) {
        long long current_time = platform_get_time_ms();
        long long delta_time = current_time - last_time;
        
        // Update keys
        chip8_update_keys(&chip8);
        
        // Execute cycles
        if (cycles_this_frame < CYCLES_PER_FRAME) {
            chip8_cycle(&chip8);
            cycles_this_frame++;
        }
        
        // Update timers and render at 60Hz
        if (delta_time >= (1000 / TARGET_FPS)) {
            chip8_update_timers(&chip8);
            chip8_render(&chip8);
            cycles_this_frame = 0;
            last_time = current_time;
        }
        
        // Small delay to prevent 100% CPU usage
        platform_sleep_ms(1);
    }
    
    printf("\nEmulation stopped.\n");
    return 0;
}