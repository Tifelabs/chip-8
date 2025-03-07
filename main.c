#include <stdio.h>
#include <stdint.h>   

#define WIDTH 64
#define HEIGHT 32

//Chip-8 specs: 4KB Memory, 16 8-bit registers
uint8_t graphics[WIDTH * HEIGHT]; // 2048 bytes, one per pixel      
uint8_t memory[4096];       // 4KB RAM
uint8_t V[16];              // General purpose registers (V0-VF)
uint16_t PC = 0x200;        // Program Counter (Starts at 0x200 for the chip-8)
uint16_t I;                 // Index register
uint8_t delay_timer;        //Timers
uint8_t sound_timer;  

//Font-Set
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

//Function to load font set
void load_fontset(){
    for(int i = 0; i < 80; i++){
        memory[i] = fontset[i];
    }
}

//Fetch Instruction
uint16_t fetch_opcode(){
    uint16_t opcode = memory[PC] << 8 | memory[PC + 1];
    PC += 2; //Next Instruction
    return opcode;
}

//Run Instruction
void execute_opcode(uint16_t opcode){
    if(opcode == 0x00E0){
        for(int i = 0; i < WIDTH * HEIGHT; i++){
            screen[i] = 0; // Clear all pixels
        }
        printf("Screen Cleared!\n");
    } else{
        printf("Unknown opcode: 0x%X\n", opcode);
    }
}


int main() {
    load_fontset();
    
    // Fake a simple program in memory (CLS: 00E0 = clear screen)
    memory[0x200] = 0x00;
    memory[0x201] = 0xE0;

    uint16_t opcode = fetch_opcode();

    printf("PC now at: 0x%X\n", PC);
    return 0;
}