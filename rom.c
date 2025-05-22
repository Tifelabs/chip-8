#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

int main() {
    FILE *file;

    file = fopen("test.ch8", "wb");

    if (file == NULL) {
        fprintf(stderr, "Could not create test.ch8\n");
        exit(1);
    }
    uint16_t program[] = {
        0x00E0, // CLS
        0x6005, // SET V0, 5 (x = 5)
        0x6105, // SET V1, 5 (y = 5)
        0xF20A, // WAIT key, store in V2
        0x8204, // ADD V0, V2 (move x by key value)
        0xA000, // SET I, 0 (font '0')
        0xD015, // DRAW V0, V1, 5
        0x1200  // JMP 0x200
    };
    fwrite(program, sizeof(uint16_t), 8, file);
    fclose(file);
    printf("[Created test.ch8]\n");
    return 0;
}