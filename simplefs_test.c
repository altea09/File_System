#include "simplefs.h"
#include <stdio.h>

#include "bitmap.h"
#include "disk_driver.h"
#include <stdlib.h>

int main(int agc, char** argv) {

    printf("\n-------Prove Size-------\n");

    printf("FirstBlock size %ld\n", sizeof(FirstFileBlock));
    printf("DataBlock size %ld\n", sizeof(FileBlock));
    printf("FirstDirectoryBlock size %ld\n", sizeof(FirstDirectoryBlock));
    printf("DirectoryBlock size %ld\n", sizeof(DirectoryBlock));

    printf("\n-------Verifica Bitmap-------\n");

    //Verifica correttezza bitmap (caso NUM_BITS_BITMAP = 16 => 2 bytes)
    int i;

    int num_blocks = NUM_BLOCKS;

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

     printf("\n-------Verifica DiskDriver-------\n");

    //Verifica delle funzioni del DiskDriver

    printf("\n-------Verifica Disk_Driver_init-------\n");

    DiskDriver* disk = (DiskDriver*) malloc(sizeof(DiskDriver));
    char* filename = "filename";
    DiskDriver_init(disk, filename, num_blocks);

    /*Funzione di stampa per testare la bitmap nel caso di file già esistente (da inserire nella funzione Disk_Driver_readDisk)
     *
     * int i;
     * for(i = 0; i < byte_bitmap * BYTE_SIZE; i++){
     * BitMapEntryKey entry_bitmap = BitMap_blockToIndex(i);
     * printf("Entry_num = %d\nBit_num = %d\nStatus = %d \n", entry_bitmap.entry_num, entry_bitmap.bit_num , ((disk->bitmap->entries[entry_bitmap.entry_num]) >> (7 - entry_bitmap.bit_num)) & 1 );
     * printf("\n");
     * }
     *
     */

     printf("\n-------Verifica Disk_Driver_writeBlock-------\n");

    int* src = (int*) malloc(BLOCK_SIZE / sizeof(int));
    for(i = 0; i < BLOCK_SIZE / sizeof(int); i++){
        if((i % 2) == 0) src[i] = 1;
        else src[i] = 0;
    }

    int scrittura = DiskDriver_writeBlock(disk, src, 3);
    if(scrittura == 0) printf("Scrittura avvenuta\n");
    else printf("Scrittura non riuscita\n");

    BitMap_print(disk->bitmap);

    printf("\n-------Verifica Disk_Driver_readBlock-------\n");

    int lettura = DiskDriver_readBlock(disk, src, 1);
    if(lettura == 0) printf("Lettura avvenuta\n");
    else printf("Lettura non riuscita\n");

    printf("\n-------Verifica Disk_Driver_getfreeBlock-------\n");

    int index_bloccoLogico = DiskDriver_getFreeBlock(disk, 4);
    printf("Indice blocco logico: %d\n", index_bloccoLogico);

    printf("\n-------Verifica Disk_Driver_freeBlock-------\n");

    int liberato = DiskDriver_freeBlock(disk, 3);
    if(liberato == -1) printf("Errore nella liberazione del blocco\n");
    else printf("Blocco liberato con successo\n");

    BitMap_print(disk->bitmap);

    printf("\n-------Verifica Disk_Driver_getfreeBlock-------\n");

    int index_bloccoLogico2 = DiskDriver_getFreeBlock(disk, 0);
    printf("Indice blocco logico: %d\n", index_bloccoLogico2);

    printf("\n-------Verifica Disk_Driver_flush-------\n");

    int flush1 = DiskDriver_flush(disk);
    if(flush1 == -1) printf("Errore nella flush\n");
    else printf("Flush fatta!\n");

    return 0;

}
