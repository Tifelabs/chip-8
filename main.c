#include <stdio.h>
#include <stdint.h>     //For clean integers

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
    

int main(){
    
    for(int i = 0; i < WIDTH; i++){
        graphics[i] = 0;
    }
    printf("Graphics initialized! Size: %d bytes\n", WIDTH * HEIGHT);
    return 0;
}