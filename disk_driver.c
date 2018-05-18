#include "disk_driver.h"
#include <fcntl.h>  //serve per i flag della open
#include <errno.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>


/* SCELTE DI PROGETTO:
 *
 * -num_block totali compreso diskheader e bitmap
 * -bitmap non inizializzata tutta a zero(i blocchi che la contengono sono a 1)
 * -diskheader e bitmap separati
 *
*/

void DiskDriver_init(DiskDriver* disk, const char* filename, int num_blocks){
    int num_block_bitmap;
    int byte_bitmap; //dimensione bitmap in byte
    int num_disco_ris; //numero dei blocchi del disco per DiskHeader + bitmap
    int seek1;
    int seek2;

    int fd = open(filename, O_RDWR | O_CREAT | O_EXCL, 0666);       //apro o creo il file a seconda di se esista già o meno
    if(fd == -1){
        if(errno == EEXIST){     //caso di file già esistente
            fd = open(filename, O_RDWR, 0666);
            void* primo_blocco;
            //Metto nel DiskDriver il file descriptor
            disk->fd = fd;
            primo_blocco = (void*) malloc(BLOCK_SIZE);
            int ret = DiskDriver_readBlockHeader(disk, primo_blocco, 0);
            if(ret == -1){
                printf("Errore nella lettura del primo blocco del file già esistente\n");
                exit(-1);
            } else{
                printf("Lettura del primo blocco già esistente andata a buon fine\n");
            }

            num_block_bitmap = (((DiskHeader*)primo_blocco)->bitmap_blocks);
            byte_bitmap = (((((DiskHeader*)primo_blocco)->num_blocks)-1)/ BYTE_SIZE) + (((((((DiskHeader*)primo_blocco)->num_blocks)-1) % BYTE_SIZE) == 0)? 0:1);
            num_disco_ris = num_block_bitmap + 1;
            int* map = mmap(0, (num_disco_ris)*BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            if (map == MAP_FAILED) {
                close(fd);
                printf("Errore nel mmapping del file\n");
                exit(-1);
            }

            disk->header = (DiskHeader*) map;
            disk->bitmap = (BitMap*)(map + BLOCK_SIZE);
            disk->bitmap->entries = (char*) (disk->bitmap + sizeof(BitMap));

        } else{
            printf("Errore nella creazione del file\n");
            exit(-1);
        }

    } else{

    seek1 = lseek(fd, (BLOCK_SIZE) -1, SEEK_SET);
    if (seek1 == -1) {
        close(fd);
        printf("Errore nella lseek\n");
        exit(-1);
    }
    seek1 = write(fd, "", 1);
    if (seek1 != 1) {
        close(fd);
        printf("Errore\n");
        exit(-1);
    }

    byte_bitmap = ((num_blocks-1) /8) +((((num_blocks-1) % 8) != 0)? 1:0);
    num_block_bitmap = ((sizeof(BitMap) + byte_bitmap)/BLOCK_SIZE) + ((((sizeof(BitMap) + byte_bitmap)%BLOCK_SIZE)==0)? 0:1);
    num_disco_ris = num_block_bitmap + 1;
    seek2 = lseek(fd, ((num_disco_ris)* BLOCK_SIZE) -1, SEEK_CUR);
    if (seek2 == -1) {
        close(fd);
        printf("Errore nella lseek\n");
        exit(-1);
    }
    seek2 = write(fd, "", 1);
    if (seek2 != 1) {
        close(fd);
        printf("Errore\n");
        exit(-1);
    }


    int* map = mmap(0, (num_disco_ris)* BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        close(fd);
        printf("Errore nel mmapping del file\n");
        exit(-1);
    }

    //Riempio il DiskHeader
    DiskHeader* disk_header = (DiskHeader*) map;
    disk_header->num_blocks = num_blocks;
    disk_header->bitmap_blocks = num_block_bitmap;
    disk_header->bitmap_entries = byte_bitmap;
    disk_header-> free_blocks = num_blocks - num_disco_ris;
    disk_header-> first_free_block = num_disco_ris;

    //Riempio il DiskDriver
    disk->header = (DiskHeader*) map;
    disk->bitmap = (BitMap*)(map + BLOCK_SIZE);
    disk->fd = fd;

    //Inizializzo la bitmap
    int ret = BitMap_initializer((BitMap*)(map + BLOCK_SIZE), byte_bitmap, num_block_bitmap);
    if(ret == -1){
        printf("Errore nella inizializzazione della bitmap\n");
        exit(-1);
    }
    else{
        printf("BitMap inizializzata correttamente\n");
    }

    disk->bitmap->entries = (char*) (disk->bitmap + sizeof(BitMap));

    }
}


int DiskDriver_readBlockHeader(DiskDriver* disk, void* dest, int index_header){
    int ret, lettura;
    if((disk == NULL) || (dest == NULL) || (index_header != 0) ){
        return -1;
        printf("Errore nella lettura\n");
    }

    ret = lseek(disk->fd, BLOCK_SIZE * index_header, SEEK_SET);
    if(ret == -1) return -1;

    lettura = read(disk->fd, dest, BLOCK_SIZE);
    if(lettura == -1){
        printf("Errore nella lettura\n");
        return -1;
    }

    return 0;
}


int DiskDriver_readBlock(DiskDriver* disk, void* dest, int block_num){      //block_num = indice del blocco. Il blocco 0 corrisponde al primo bit a 0 della bitmap (dopo i bit a 1 dei blocchi occupati dalla bitmap => blocco 0 logico)
    int ret, lettura, indice_blocco;

    if((disk == NULL) || (dest == NULL) || (block_num > (((disk->header->num_blocks)- 1 - disk->header->bitmap_blocks))) || (block_num < 0)){
        return -1;
        printf("Impossibile leggere\n");
    }

    indice_blocco = block_num + disk->header->bitmap_blocks;
    BitMapEntryKey bek = BitMap_blockToIndex(indice_blocco);
    if(!((disk->bitmap->entries[bek.entry_num]) >> (7 - bek.bit_num) & 1)){   // verifico se il blocco è occupato: se no, restituisco errore nella lettura.
        printf("Errore: si sta tentando di leggere un blocco vuoto\n");
        return -1;
    }

    ret = lseek(disk->fd, BLOCK_SIZE * ((disk->header->bitmap_blocks) + 1 + block_num), SEEK_SET);
    if(ret == -1) return -1;


    lettura = read(disk->fd, dest, BLOCK_SIZE);
    if(lettura == -1){
        printf("Errore nella lettura\n");
        return -1;
    }

    return 0;
}


int DiskDriver_writeBlock(DiskDriver* disk, void* src, int block_num){
    int ret, indice_blocco, scrittura, bmap;

    if((disk == NULL) || (src == NULL) || (block_num > (((disk->header->num_blocks)- 1 - disk->header->bitmap_blocks)))|| (block_num < 0)){
        printf("Tentativo di scrittura fallito\n");
        return -1;
    }

    indice_blocco = block_num + disk->header->bitmap_blocks;    //indice all'interno della bitmap
    BitMapEntryKey bek = BitMap_blockToIndex(indice_blocco);
    if(((disk->bitmap->entries[bek.entry_num]) >> (7 - bek.bit_num) & 1)){   // verifico se il blocco è vuoto: se no, restituisco errore nella scrittura.
        printf("Errore: si sta tentando di scrivere in un blocco pieno\n");
        return -1;
    }

    ret = lseek(disk->fd, BLOCK_SIZE * ((disk->header->bitmap_blocks) + 1 + block_num), SEEK_SET);
    if(ret == -1) return -1;

    scrittura = write(disk->fd, src, BLOCK_SIZE);
    if(scrittura == -1){
        printf("Errore nella scrittura\n");
        return -1;
    }


    //se la scrittura è andatata a buon fine aggiorno la bitmap.
    bmap = BitMap_set(disk->bitmap, indice_blocco, 1);
    if(bmap != 0){
        printf("Errore nel settaggio della bitmap\n");
        return -1;
    }

    //Decremento il numero di blocchi liberi.
    disk->header->free_blocks -=1;

    BitMap_print(disk->bitmap);

    return 0;

}


int DiskDriver_freeBlock(DiskDriver* disk, int block_num){

    int indice_blocco, bmap;

    if((disk == NULL) || (block_num >= (((disk->header->num_blocks)-1 - disk->header->bitmap_blocks))) || (block_num < 0)){
        printf("Errore\n");
        return -1;
    }

    indice_blocco = block_num + disk->header->bitmap_blocks;

    BitMapEntryKey bek = BitMap_blockToIndex(indice_blocco);
    if(!((disk->bitmap->entries[bek.entry_num]) >> (7 - bek.bit_num) & 1)){   // verifico se il blocco è occupato: se no, restituisco errore.
        printf("Errore: si sta tentando di svuotare un blocco vuoto\n");
        return -1;
    }

    //faccio risultare libero il blocco nella bitmap settando a 0
    bmap = BitMap_set(disk->bitmap, indice_blocco, 0);
    if(bmap != 0){
        printf("Errore nel settaggio della bitmap\n");
        return -1;
    }

    //Aggiorno il numero di blocchi liberi
    disk->header->free_blocks += 1;

    if(disk->header->first_free_block == -1){
        disk->header->first_free_block = indice_blocco;     //indice del primo blocco libero all'interno della bitmap (NON è l'indice del blocco logico!)
    }
    if(indice_blocco < disk->header->first_free_block){      //se il blocco liberato è minore del primo blocco libero devo aggiornare il first free block
        disk->header->first_free_block = indice_blocco;
    }

    BitMap_print(disk->bitmap);

    return 0;

}


int DiskDriver_getFreeBlock(DiskDriver* disk, int start){
    //start è l'indice nella bitmap (se volessi considerlo come indice del blocco logico, basterebbe passare start + num_block_bitmap a start)


    int indice_blocco;

    if((disk == NULL) || (start < 0) || (start >= ((disk->header->num_blocks)-1))){
        printf("Impossibile restituire il blocco\n");
        return -1;
    }

    int free_block_index = BitMap_get(disk->bitmap, start, 0); //restituisce l'indice all'interno della bitmap
    if(free_block_index == -1){
        printf("Non è stato trovato alcun blocco libero\n");
        return -1;
    }

    indice_blocco = free_block_index - disk->header->bitmap_blocks;     //indice del blocco logico (il primo blocco libero per i dati ha indice logico 0)
    disk->header->first_free_block = indice_blocco + disk->header->bitmap_blocks;   //indice all'interno della bitmap
    return indice_blocco;    //restituisce l'indice del primo blocco logico libero

}


int DiskDriver_flush(DiskDriver* disk){

    int num_disco_ris, num_block_bitmap, flush, file;

    if(disk == NULL) return -1;

    int byte_bitmap = (disk->header->num_blocks / 8) + ((disk->header->num_blocks % 8) != 0)? 1:0;

    num_block_bitmap = ((sizeof(BitMap) + byte_bitmap)/BLOCK_SIZE) + ((((sizeof(BitMap) + byte_bitmap) % BLOCK_SIZE) == 0)? 0:1);
    num_disco_ris = num_block_bitmap + 1;

    flush = msync(disk->header, num_disco_ris * BLOCK_SIZE, MS_SYNC);
    if(flush == -1) return -1;

    file = fsync(disk->fd);
    if(file == -1) return -1;

    return 0;

}
