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




