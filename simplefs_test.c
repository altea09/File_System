#include "simplefs.h"
#include <stdio.h>

#include "bitmap.h" //aggiunto
#include <stdlib.h> //aggiunto

int main(int agc, char** argv) {

    printf("\n-------Prove Size-------\n");

    printf("FirstBlock size %ld\n", sizeof(FirstFileBlock));
    printf("DataBlock size %ld\n", sizeof(FileBlock));
    printf("FirstDirectoryBlock size %ld\n", sizeof(FirstDirectoryBlock));
    printf("DirectoryBlock size %ld\n", sizeof(DirectoryBlock));

    printf("\n-------Verifica Bitmap-------\n");

    //Verifica correttezza bitmap (caso NUM_BITS_BITMAP = 16 => 2 bytes)
    int i;
    int nbytes = (NUM_BITS_BITMAP / BYTE_SIZE) + ((NUM_BITS_BITMAP % BYTE_SIZE == 0)? 0 : 1);  //calcolo il numero necessario di byte per contenere la bitmap
    printf("Numero bytes bitmap: %d\n", nbytes);
    char* b = (char*) malloc(nbytes * sizeof(char)); //alloco i byte necessari

    BitMap bmap;
    bmap.num_bits = NUM_BITS_BITMAP;
    bmap.entries =  b;

    for(i = 0; i < NUM_BITS_BITMAP / BYTE_SIZE; i++){
        if((i % 2) == 0){
            b[i] = '1';
        } else {
            b[i] = '0';
        }
        printf("Byte[%d]: %d\n",i, b[i]);
    }

    if(NUM_BITS_BITMAP % BYTE_SIZE != 0){
        b[nbytes-1] = 0;
        printf("Byte[%d]: %d\n", nbytes -1, b[nbytes -1]);
    }

    printf("\n");
    BitMap_print(&bmap);


    printf("\n-------Verifica prima funzione Bitmap-------\n");

    BitMapEntryKey bek = BitMap_blockToIndex(9);
    printf("Entry_num: %d, Bit_num: %d\n", bek.entry_num, bek.bit_num);

    printf("\n-------Verifica seconda funzione Bitmap-------\n");

    int index = BitMap_indexToBlock(1,1);
    printf("Indice: %d\n", index);

    printf("\n-------Verifica terza funzione Bitmap-------\n");

    int index_bit = BitMap_get(&bmap,2,0);
    printf("Indice del primo bit a 'status': %d\n", index_bit);

    printf("\n-------Verifica quarta funzione Bitmap-------\n");

    int ret = BitMap_set(&bmap,7,0);
    printf("Ret = %d\n", ret);

    BitMap_print(&bmap);
    printf("\n");

    return 0;

}
