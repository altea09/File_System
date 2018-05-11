#include "bitmap.h"

#include <stdio.h>

// ogni elemento di BitMapEntryKey (entry_num e bit_num) indica il blocco da sinistra.

void BitMap_print(BitMap* bmap){
    int i;

    for(i = 0; i < bmap->num_bits; i++){
        BitMapEntryKey entry_bitmap = BitMap_blockToIndex(i);
        printf("Entry_num = %d\nBit_num = %d\nStatus = %d \n", entry_bitmap.entry_num, entry_bitmap.bit_num , ((bmap->entries[entry_bitmap.entry_num]) >> (7 - entry_bitmap.bit_num)) & 1 );
        printf("\n");
    }

}

BitMapEntryKey BitMap_blockToIndex(int num){

    BitMapEntryKey bitmap_entry;

    if(num > NUM_BITS_BITMAP -1){   //verifico che l'indice passato alla funzione rientri negli indici della bitmap
        printf("Indice del blocco non valido\n");
        bitmap_entry.entry_num = -1;
        bitmap_entry.bit_num = -1;
    } else {
        bitmap_entry.entry_num = num / BYTE_SIZE;
        bitmap_entry.bit_num = (num % BYTE_SIZE);   // ogni sequenza di bit ha indice di partenza 0
    }

    return bitmap_entry;

}

int BitMap_indexToBlock(int entry, char bit_num){

    if(entry > (NUM_BITS_BITMAP / BYTE_SIZE)){
        printf("Errore\n");
        return -1;
    }

    int nbytes = (NUM_BITS_BITMAP / BYTE_SIZE) + ((NUM_BITS_BITMAP % BYTE_SIZE == 0)? 0 : 1); //considero un byte in più se c'è il resto
    if(((NUM_BITS_BITMAP % BYTE_SIZE) != 0) && (entry == nbytes-1)){    //non posso lavorare nella bitmap sui bit dell'ultimo byte allocato che eccedono il numero di blocchi esistenti.
        if(bit_num > ((NUM_BITS_BITMAP % BYTE_SIZE) - 1)){
            printf("Indice non valido\n");
        }
    }

    int index = (entry * BYTE_SIZE) + bit_num;
    return index;

}
