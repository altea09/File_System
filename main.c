#include "simplefs.h"
#include <stdio.h>

#include "bitmap.h"
#include "disk_driver.h"
#include <stdlib.h>

void stato_disco(DiskDriver* disk){
    printf("\n-------Verifica Stato del disco-------\n");

    printf("Numero dei blocchi totali (compreso DiskHeader riservato): %d\n", disk->header->num_blocks);
    printf("Numero dei blocchi dei dati (compresa bitmap): %d\n", disk->header->num_blocks -1);
    printf("Numero dei blocchi liberi: %d\n", disk->header->free_blocks);
}

int main(int agc, char** argv) {


    SimpleFS* sfs = (SimpleFS*) malloc(sizeof(SimpleFS));

    DiskDriver* disk = (DiskDriver*) malloc(sizeof(DiskDriver));

    char* filename = "filename";
    int num_blocks = NUM_BLOCKS;

    sfs->disk= disk;
    sfs->file_name= filename;
    sfs->num_blocks= num_blocks;

    printf("\n-------Verifica SimpleFS_format-------\n");

    SimpleFS_format(sfs);

    printf("\n");
    BitMap_print(sfs->disk->bitmap);

    printf("\n-------Verifica SimpleFS_init + DiskDriver_readDisk-------\n");

    int ret = DiskDriver_readDisk(sfs->disk, sfs->file_name);
    if(ret == -1) {
        return -1;
    }

    DirectoryHandle* dir = SimpleFS_init(sfs, disk);
    BitMap_print(sfs->disk->bitmap);
    stato_disco(sfs->disk);

    printf("\n-------Verifica SimpleFS_createFile-------\n");

    FileHandle* fh =(FileHandle*) malloc(sizeof(FileHandle));
    char* file = "file1";

    fh = SimpleFS_createFile(dir, file);

    BitMap_print(sfs->disk->bitmap);

    printf("Fh: %p\n", fh);
    stato_disco(sfs->disk);

    printf("\n\n");

    FileHandle* fh2 = (FileHandle*) malloc(sizeof(FileHandle));
    fh2 = SimpleFS_createFile(dir, file);
    printf("Dal momento che esiste già un altro file con lo stesso nome la funzione ritorna NULL\n");
    printf("Fh2: %p\n", fh2);

    BitMap_print(sfs->disk->bitmap);
    stato_disco(sfs->disk);

     printf("\n-------Verifica SimpleFS_readDir-------\n");

    char* names;
    int leggi_Dir = SimpleFS_readDir(&names, dir);
    if(leggi_Dir == -1){
        printf("Lettura dei nomi non avvenuta\n");
    } else{
        printf("Lettura dei nomi avvenuta con successo\n");
    }

    printf("Nomi: \n %s", names);

    free(names);

    printf("\n-------Verifica SimpleFS_close-------\n");

    int chiusura =SimpleFS_close(fh);
    if(chiusura == -1){
        printf("Errore nella chiusura\n");
        return -1;
    }

    printf("\n-------Verifica SimpleFS_openFile-------\n");

    fh2 = SimpleFS_openFile(dir, file);
    printf("Fh2: %p\n", fh2);


    return 0;
}
