#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "font_set/fontset.h"        //Fontset
#include "opcode/opcode.h"        //Operation code


#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#else
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/time.h>
#endif

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

// Timing Constants
#define TARGET_FPS 60
#define CYCLES_PER_SECOND 700
#define CYCLES_PER_FRAME (CYCLES_PER_SECOND / TARGET_FPS)

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