#include <unistd.h> // Para sbrk
#include "mm_malloc.h"
#include <string.h>
#include <stdint.h>
// Inicio de la lista enlazada del heap
void *base = NULL;

void *my_malloc(size_t size) {
    if (size == 0) return NULL; // no tiene sentido crear un bloque de tamaño 0

    size = (size + 7) & ~7; // Alineación a 8 bytes

    // Si es el primer bloque 
    if (base == NULL) {
        
        void *block = sbrk(META_SIZE + size); //le pido mas memoria al kernel 
        if (block == (void*) -1) return NULL; // si falla sbrk devuelvo NULL

        block_meta *current = (block_meta*)block; // la direccion de block se interpreta como un block_meta

        //Inicializacion de los meta-datos 
        current->size = size;
        current->next = NULL;
        current->free = 0;
        current->magic = 0x12345678;

        base = current;

        return (void*)(current + 1); // retornamos un void* que apunta al inicio del payload
    }

    block_meta *current = base; //inico en la base de la lista 
    block_meta *prev = NULL; //usado para guardar el bloque anterior en el recorrido 

    // Buscar bloque libre
    while (current != NULL) {
        
        if (current->free && current->size >= size) {
            
            /*--------------SPLITTING--------------*/
            if (current->size >= size + META_SIZE + 8) {
                
                // Crear nuevo bloque en la parte restante
                block_meta *new_block = (block_meta*)((char*)(current + 1) + size); // Usamos char* para hacer aritmética en bytes y posicionarnos exactamente después del payload

                new_block->size = current->size - size - META_SIZE;
                new_block->next = current->next;
                new_block->free = 1;
                new_block->magic = 0x12345678;

                // Ajustar bloque actual
                current->size = size;
                current->next = new_block;
            }

            current->free = 0;
            current->magic = 0x12345678;
            return (void*)(current + 1);
        }

        prev = current;
        current = current->next;
    }

    // No se encontró bloque libre → crear nuevo
    void *block = sbrk(META_SIZE + size);
    if (block == (void*) -1) return NULL;

    block_meta *new_block = (block_meta*)block;

    new_block->size = size;
    new_block->next = NULL;
    new_block->free = 0;
    new_block->magic = 0x12345678;

    // Enlazar al final de la lista
   if (prev != NULL) {
        prev->next = new_block;
    }


    return (void*)(new_block + 1);
}

void my_free(void *ptr) 
{

    block_meta *free_block = (block_meta*)ptr - 1; // creo el bloque apuntando a los meta-datos 

    if (free_block->magic != 0x12345678) return; //verifico que sea un bloque de memoria asignado por my_malloc
    
    free_block->free = 1;
    free_block->magic = 0xDEADBEEF; // como el bloque es libre cambio el valor de magic para evitar double free
    
    /*------------COALESCING------------*/
    
    //Evalua si el bloque previo esta libre y si lo esta los fusiona

    block_meta *current = base;
    block_meta *prev = NULL;

    while (current != free_block) 
    {
        prev = current;
        current = current->next;
    }


    if (prev && prev->free) 
    {
        prev->size += META_SIZE + free_block->size;
        prev->next = free_block->next;
        free_block = prev;
    }

    //Evalua si el bloque siguiente esta libre y si lo esta los fusiona
    
    if (free_block->next && free_block->next->free) 
    {
        free_block->size += META_SIZE + free_block->next->size;
        free_block->next = free_block->next->next;
    }   


}

void *my_calloc(size_t nmemb, size_t size) 
{
    if (nmemb == 0 || size == 0) return NULL; 
    if (size != 0 && nmemb > SIZE_MAX / size) return NULL; // medida para evitar overflow 
    
    size_t total_size = nmemb * size;
    
    void *ptr = my_malloc(total_size); // reservo memoria
    
    if(ptr != NULL) 
    {
        void* result = memset(ptr, 0, total_size); // todo lleno toda la memoria de ceros 
        return result; 
    }
    return NULL;
    
}

void *my_realloc(void *ptr, size_t size) 
{
    if(ptr == NULL) return my_malloc(size); // Si ptr es NULL, realloc se comporta como malloc
    if(size == 0) 
    {
        my_free(ptr); // Si size es 0, realloc se comporta como free
        return NULL;
    }
    
    size = (size + 7) & ~7;
    
    block_meta *block = (block_meta*)ptr - 1;
    
    if (block->magic != 0x12345678) return NULL;
        
    
    if(block->size >= size)  // Verifica que tamaño es mayor si el original o el nuevo 
    {
         /*--------------SPLITTING--------------*/
        if (block->size >= size + META_SIZE + 8)
        {
            block_meta *new_block = (block_meta*)((char*)(block + 1) + size);

            new_block->size = block->size - size - META_SIZE;
            new_block->next = block->next;
            new_block->free = 1;
            new_block->magic = 0x12345678;

            block->size = size;
            block->next = new_block;
        }
        // Si el bloque no es lo suficientemente grande para dividirlo retorno el puntero sin mayor proceso 
        return ptr; 
    }
    else
    {
        // Copiamos los datos del bloque original al nuevo.
        // Solo se copian block->size bytes (truncamiento si el nuevo tamaño es menor).
        void* new_ptr = my_malloc(size);
        if (new_ptr == NULL) return NULL;
        memcpy(new_ptr, ptr, block->size);
        my_free(ptr);
        return new_ptr;

    }
    
    
}
