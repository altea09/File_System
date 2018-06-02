#include "simplefs.h"
#include "disk_driver.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h> // serve per memcpy

DirectoryHandle* SimpleFS_init(SimpleFS* fs, DiskDriver* disk){
    int ret;

    if((fs == NULL) || (disk == NULL)) return NULL;

    fs->disk = disk;

    FirstDirectoryBlock* dcb = (FirstDirectoryBlock*) malloc(sizeof(FirstDirectoryBlock));

    ret = DiskDriver_readBlock(fs->disk, dcb, 0); //leggo il primo blocco logico che è il first directory block della directory root.
    if(ret == -1){
        printf("Errore nella lettura\n");
        free(dcb);
        return NULL;
    }

    //Creo il directory handle
    DirectoryHandle* directory_handle = (DirectoryHandle*) malloc(sizeof(DirectoryHandle));
    directory_handle->sfs = fs;
    directory_handle->dcb = dcb;
    directory_handle->directory = NULL;
    directory_handle->current_block = (BlockHeader*) malloc(sizeof(BlockHeader));
    memcpy(directory_handle->current_block, &(dcb->header), sizeof(BlockHeader));
    directory_handle->pos_in_dir = 0;
    directory_handle->pos_in_block = 0;

    return directory_handle;

}

void SimpleFS_format(SimpleFS* fs){
    int index_first_freeBlock, i, ret;
    int num_file_blocks=0;

    if(fs == NULL){
        printf("Puntatore al file system non valido\n");
        return;
    }

    DiskDriver_init(fs->disk, fs->file_name, fs->num_blocks);
    //cerca un blocco libero
    index_first_freeBlock = DiskDriver_getFreeBlock(fs->disk, 0);
    if(index_first_freeBlock == -1) return;

    //Riempio il block header
    FirstDirectoryBlock* dcb = (FirstDirectoryBlock*) malloc(sizeof(FirstDirectoryBlock));
    dcb->header.previous_block = -1;
    dcb->header.next_block = -1;
    dcb->header.block_in_file = 0;

    //Riempio il file control block
    dcb->fcb.directory_block = -1;
    dcb->fcb.block_in_disk = index_first_freeBlock; //indice blocco logico
    strncpy(dcb->fcb.name, "/", 2);
    dcb->fcb.size_in_bytes = sizeof(FirstDirectoryBlock);
    dcb->fcb.size_in_blocks = 1;
    dcb->fcb.is_dir = 1; //1 indica directory

    //Finisco di riempire il First Directory Block
    dcb->num_entries = 0;

    //Calcolo quanti elementi ha l'array di interi del firstDirectoryBlock
    num_file_blocks = ((BLOCK_SIZE - sizeof(BlockHeader) - sizeof(FileControlBlock) - sizeof(int)) / sizeof(int));

    //Inizializzo l'array in modo che tutti gli elementi siano a -1 (indicano che la directory non ha elementi)
    for(i = 0; i < num_file_blocks; i++){
        dcb->file_blocks[i] = -1;
    }

    ret = DiskDriver_writeBlock(fs->disk, (void*) dcb, index_first_freeBlock);
    if(ret == -1){
        free(dcb);
        return;
    }
    free(dcb);

}

FileHandle* SimpleFS_createFile(DirectoryHandle* d, const char* filename){
    int index_free_block, scrittura, ret;

    if((d == NULL) || (filename == NULL) || (strlen(filename) == 0)) return NULL;

    //verifico se il file esiste già nella directory
    int esiste = SimpleFS_Search(d, filename, 0);
    if(esiste != -1){
        printf("Esiste già un file con lo stesso nome nella directory\n");
        return NULL;
    }

    //verifico l'esistenza di un blocco libero
    if(d->sfs->disk->header->free_blocks < 1) return NULL;

    //caso in cui il file non esista già nella directory-->devo trovare un blocco libero
    index_free_block = DiskDriver_getFreeBlock(d->sfs->disk, 0);
    if(index_free_block == -1){
        //verifica ulteriore del valore di ritorno (fatta più per sicurezza che per necessità)
        printf("Non è stato trovato nessun blocco libero\n");
        return NULL;
    }

    //Creo il primo blocco del file
    FirstFileBlock* first_file_block = (FirstFileBlock*) malloc(sizeof(FirstFileBlock));
    first_file_block->header.previous_block = -1; //essendo il primo blocco del file non ha un precedente
    first_file_block->header.next_block = -1; //file vuoto...non ha un successore
    first_file_block->header.block_in_file = 0; //0 perchè è il file control block

    //Riempio il file control block
    first_file_block->fcb.directory_block = d->dcb->fcb.block_in_disk;
    first_file_block->fcb.block_in_disk = index_free_block;
    strncpy(first_file_block->fcb.name, filename, strlen(filename));  //uso strncpy perchè più sicura di strcpy
    first_file_block->fcb.size_in_bytes = 0;
    first_file_block->fcb.size_in_blocks = 1; //ho messo 1 perchè almeno un blocco è occupato dal file (blocco del first file block)
    first_file_block->fcb.is_dir =0; //0 indica file

    //scrivo il file su disco
    scrittura = DiskDriver_writeBlock(d->sfs->disk, first_file_block, index_free_block);
    if(scrittura == -1){
        printf("Scrittura fallita\n");
        return NULL;
    }


    //aggiorno il numero di entry(elementi della cartella) nella directory
    ret = SimpleFS_updateElements(d, index_free_block);
    if(ret == -1){
        printf("Problema nell'aggiornamento degli elementi\n");
        exit(-1); //esco perchè senza l'aggiornamento ci sarebbe un'inconsistenza dei dati
    }

    //E' necessario creare un file handle che "gestisca" il file
    FileHandle* file_handle = (FileHandle*) malloc(sizeof(FileHandle));
    file_handle->sfs = d->sfs;
    file_handle->fcb = first_file_block;
    file_handle->directory = (FirstDirectoryBlock*) malloc(sizeof(FirstDirectoryBlock));
    memcpy(file_handle->directory, d->dcb, sizeof(FirstDirectoryBlock));
    file_handle->current_block = (BlockHeader*) malloc(sizeof(BlockHeader));
    memcpy(file_handle->current_block, &(first_file_block->header), sizeof(BlockHeader));
    file_handle->pos_in_file = 0; // posizione del cursore dopo le dimensioni di blockheader e file control block

    return file_handle;

}

//restituisce -1 se non trova il file o è successo qualche altro problema, il primo blocco del file su disco se lo trova (flag = 1 cerco directory, flag = 0 cerco file)
int SimpleFS_Search(DirectoryHandle* d, const char* filename, int flag){
    int num_entry_dir, num_blocks_fdb, i, ret, next_dir_block, num_blocks_db;

    if((d == NULL) ||(filename == NULL) || ((strlen(filename)) == 0))return -1;

    int first_file_block = -1; //flag per capire se un file con lo stesso nome è stato trovato (-1= non trovato) -->se trovato il blocco su disco

    //verifico che ci siano elementi nella directory
    num_entry_dir = d->dcb->num_entries;
    if(num_entry_dir == 0) return first_file_block;

    num_blocks_fdb = ((BLOCK_SIZE - sizeof(BlockHeader)- sizeof(FileControlBlock) - sizeof(int))/sizeof(int));
    num_blocks_db = ((BLOCK_SIZE - sizeof(BlockHeader) - sizeof(int))/sizeof(int));

    int cosa_cerco = flag; //setto cosa devo cercare


    FirstFileBlock* ffb =(FirstFileBlock*) malloc(sizeof(FirstFileBlock));
    for(i=0; (i < num_entry_dir) && (first_file_block == -1) && (i < num_blocks_fdb); i++){

        ret = DiskDriver_readBlock(d->sfs->disk, ffb, d->dcb->file_blocks[i]);
        if(ret == -1){
            printf("Lettura non avvenuta con successo\n");
            free(ffb);
            return -1;
        }

        if((strncmp(ffb->fcb.name, filename, strlen(filename))) == 0){
            if(ffb->fcb.is_dir == cosa_cerco){
                printf("Trovato un/a file/direcotry con lo stesso nome\n");
                first_file_block = d->dcb->file_blocks[i];
                free(ffb);
                return first_file_block;
            }

        }
    }

    next_dir_block = d->dcb->header.next_block;

    while(next_dir_block != -1){

        DirectoryBlock* db = (DirectoryBlock*) malloc(sizeof(DirectoryBlock));

        ret = DiskDriver_readBlock(d->sfs->disk, db, next_dir_block);
        if(ret == -1){
            printf("Errore nella lettura\n");
            free(db);
            free(ffb);
            return -1;
        }


        for(i=0; (i < num_entry_dir) && (first_file_block ==-1) && (i < num_blocks_db); i++){

            ret = DiskDriver_readBlock(d->sfs->disk, ffb, db->file_blocks[i]);
            if(ret == -1){
                printf("Lettura non avvenuta con successo\n");
                free(ffb);
                free(db);
                return -1;
            }
            if((strncmp(ffb->fcb.name, filename, strlen(filename))) == 0){
                if(ffb->fcb.is_dir == cosa_cerco){
                    printf("Trovato un/a file/directory con lo stesso nome\n");
                    first_file_block = db->file_blocks[i];
                    free(db);
                    free(ffb);
                    return first_file_block;
                }
            }
        }
        next_dir_block = db->header.next_block;

    }
    return first_file_block;

}

int SimpleFS_updateElements(DirectoryHandle* d, int new_block_file){
    int num_blocks_fdb, num_blocks_db, ret, i, next_block, block_in_disk, conta, index, index_free_block;

    if((d == NULL) || (new_block_file < 1)) return -1; //minore di 1 perchè nel blocco 0 logico (bit !=0 bitmap perchè prima ci sono gli 1 della bitmap stessa!)c'è la directory root sicuramente

    num_blocks_fdb = ((BLOCK_SIZE - sizeof(BlockHeader) - sizeof(FileControlBlock) - sizeof(int)) / sizeof(int));
    num_blocks_db = (((BLOCK_SIZE - sizeof(BlockHeader))) / sizeof(int)); //numero di celle dell'array di un directory block

    DirectoryBlock* db = (DirectoryBlock*) malloc(sizeof(DirectoryBlock));

    memcpy(d->current_block, &(d->dcb->header), sizeof(BlockHeader));
    block_in_disk = d->dcb->fcb.block_in_disk;
    int spazio_vuoto = 0; // flag per vedere se ho trovato un buco
    for(i=0; i < num_blocks_fdb && spazio_vuoto ==0; i++){
        if(d->dcb->file_blocks[i] == -1){
            d->dcb->file_blocks[i] = new_block_file;
            spazio_vuoto = 1;
            d->pos_in_block = i;
            d->pos_in_dir = i+1;
        }
    }

    if(spazio_vuoto == 0){
        conta = 0;
        DirectoryBlock* db = (DirectoryBlock*) malloc(sizeof(DirectoryBlock));
        while((d->current_block->next_block != -1) && (spazio_vuoto == 0)){
            next_block = d->current_block->next_block;
            ret = DiskDriver_readBlock(d->sfs->disk, db, next_block);
            if(ret == -1){
                printf("Lettura fallita\n");
                return -1;
            }
            memcpy(d->current_block, &(db->header), sizeof(BlockHeader));
            for(i=0; i < num_blocks_db && spazio_vuoto == 0; i++){
                if(db->file_blocks[i] == -1){
                    db->file_blocks[i] = new_block_file;
                    spazio_vuoto = 1;
                    d->pos_in_block = i;
                    d->pos_in_dir = (i+1) + num_blocks_fdb + (num_blocks_db * conta);
                    block_in_disk = next_block;
                }
            }
            ++conta;
        }
        if(spazio_vuoto == 0){
            index_free_block = DiskDriver_getFreeBlock(d->sfs->disk, 0);
            if(index_free_block == -1){
                printf("Non è stato trovato nessun blocco libero\n");
                return -1;
            }
            DirectoryBlock* db2 = (DirectoryBlock*) malloc(sizeof(DirectoryBlock));

        //inizializzo il directory block
        db2->header.previous_block = next_block;
        db2->header.next_block = -1;
        db2->header.block_in_file = (db->header.block_in_file) +1;
        index =0;

        for(i=0; i < num_blocks_db; i++){
            db2->file_blocks[i] = -1;
        }

        db2->file_blocks[index] = new_block_file;
        d->pos_in_block = index;
        db->header.next_block = index_free_block;
        memcpy(d->current_block, &(db2->header), sizeof(BlockHeader));

        ret = DiskDriver_writeBlock(d->sfs->disk, db2, index_free_block);
        if(ret == -1){
            printf("Errore\n");
            free(db);
            free(db2);
            return -1;
        }
        free(db2);
        }
    }

    d->dcb->num_entries = (d->dcb->num_entries) + 1; //aggiorno il numero di entry visto il file in più

    if(block_in_disk != d->dcb->fcb.block_in_disk){
        ret = DiskDriver_writeBlock(d->sfs->disk, db, block_in_disk);
        if(ret == -1){
            printf("Errore nell'aggiornamento del directory block\n");
            return -1;
        }
    }

    //aggiorno il contenuto del first directory block
    ret = DiskDriver_writeBlock(d->sfs->disk, d->dcb, d->dcb->fcb.block_in_disk);
    if(ret == -1){
        printf("Errore nell'aggiornamento del first directory block\n");
        return -1;
    }

    return 0;

}

int SimpleFS_readDir(char** names, DirectoryHandle* d){
     int num_entry, dim, num_blocks_fdb, ret, i, num_blocks_db, next_dir_block;

    if(d == NULL) return -1;

    //Calcolo il numero di entry della directory
    num_entry = d->dcb->num_entries;

    if(num_entry == 0){
        printf("La directory è vuota: non ci sono nomi di file da leggere\n");
        return -1;
    }

    //Calcolo la dimensione (al massimo mi servirà tanto spazio quanto è la lunghezza massima del nome * il numero di entries
    dim = ((num_entry)* 128);
    *names = (char*) malloc(dim);

    num_blocks_fdb = BLOCK_SIZE - sizeof(BlockHeader) - sizeof(FileControlBlock);


    memset(*names, 0,dim);
    FirstFileBlock* ffb = (FirstFileBlock*) malloc(sizeof(FirstFileBlock));
    for(i = 0; (i < num_blocks_fdb) && (i < num_entry); i++){
        ret = DiskDriver_readBlock(d->sfs->disk, ffb, d->dcb->file_blocks[i]);
        if(ret == -1){
            printf("Lettura fallita\n");
            free(ffb);
            return -1;
        }
        strncat((*names)+ strlen(*names), ffb->fcb.name, strlen(ffb->fcb.name));
        strcat((*names), "\n"); //lo metto per andare a capo dopo ogni nome
    }

    num_blocks_db = (BLOCK_SIZE - sizeof(BlockHeader)) / sizeof(int);

    DirectoryBlock* db = (DirectoryBlock*) malloc(sizeof(DirectoryBlock));
    next_dir_block = d->dcb->header.next_block;

    while(next_dir_block != -1){
        ret = DiskDriver_readBlock(d->sfs->disk, db, next_dir_block);
        if(ret == -1){
            printf("Errore nella lettura\n");
            free(db);
            free(ffb);
            return -1;
        }

        for(i=0; (i < num_entry) && (i < num_blocks_db); i++){
            ret = DiskDriver_readBlock(d->sfs->disk, ffb, db->file_blocks[i]);
            if(ret == -1){
                printf("Lettura fallita\n");
                free(ffb);
                free(db);
                return -1;
            }
            strncat((*names)+ strlen(*names), ffb->fcb.name, strlen(ffb->fcb.name));
            strcat((*names), "\n");
        }
        next_dir_block = db->header.next_block;
    }

    return 0;
}

FileHandle* SimpleFS_openFile(DirectoryHandle* d, const char* filename){
    int first_file_block, ret;

    if((d == NULL) || (strlen(filename) == 0)) return NULL;

    //verifico che il file esista
    first_file_block = SimpleFS_Search(d, filename,0);
    if(first_file_block == -1){
        printf("Il File desiderato non esiste\n");
        return NULL;
    }

    //Se il file esiste e quindi può essere aperto, è necessario creare un file handle
    FileHandle* file_handle = (FileHandle*) malloc(sizeof(FileHandle));
    file_handle->sfs = d->sfs;
    FirstFileBlock* ffb = (FirstFileBlock*) malloc(sizeof(FirstFileBlock));

    ret = DiskDriver_readBlock(d->sfs->disk, ffb, first_file_block);
    if(ret == -1){
        printf("Errore nella lettura\n");
        free(file_handle);
        free(ffb);
        return NULL;

    }

    file_handle->fcb = (FirstFileBlock*)malloc(sizeof(FirstFileBlock));
    memcpy(file_handle->fcb, ffb, sizeof(FirstFileBlock));
    file_handle->directory = (FirstDirectoryBlock*) malloc(sizeof(FirstDirectoryBlock));
    memcpy(file_handle->directory, d->dcb, sizeof(FirstDirectoryBlock));
    file_handle->current_block = (BlockHeader*) malloc(sizeof(BlockHeader));
    memcpy(file_handle->current_block, &(ffb->header),sizeof(BlockHeader));
    file_handle->pos_in_file =0;

    free(ffb);

    printf("File aperto con successo!\n");

    return file_handle;

}

int SimpleFS_close(FileHandle* f){
    if(f == NULL) return -1;

    //Libero la memoria allocata nell'apertura del file
    free(f->fcb);
    free(f->directory);
    free(f->current_block);
    free(f);

    printf("Chiusura avvenuta con successo!\n");
    return 0;
}

