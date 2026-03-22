#include <unistd.h> // Para sbrk
#include "mm_malloc.h"
#include <string.h>
#include <stdint.h>
// Inicio de la lista enlazada del heap
void *base = NULL;

void *my_malloc(size_t size) {
    if (size == 0) return NULL;

    size = (size + 7) & ~7;

    // Si es el primer bloque
    if (base == NULL) {
        void *block = sbrk(META_SIZE + size);
        if (block == (void*) -1) return NULL;

        block_meta *current = (block_meta*)block;

        current->size = size;
        current->next = NULL;
        current->free = 0;
        current->magic = 0x12345678;

        base = current;

        return (void*)(current + 1);
    }

    block_meta *current = base;
    block_meta *prev = NULL;

    // Buscar bloque libre
    while (current != NULL) {
        
        if (current->free && current->size >= size) {
            if (current->size >= size + META_SIZE + 8) {
                
                // Crear nuevo bloque en la parte restante
                block_meta *new_block = (block_meta*)((char*)(current + 1) + size);

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

    block_meta *free_block = (block_meta*)ptr - 1;

    if (free_block->magic != 0x12345678) return;
    
    free_block->free = 1;
    free_block->magic = 0xDEADBEEF;
    
    /*------------Coalescing------------*/
    
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
    if (size != 0 && nmemb > SIZE_MAX / size) return NULL;
    size_t Array_size = nmemb * size;
    
    void *ptr = my_malloc(Array_size);
    if(ptr != NULL)
    {
        void* result = memset(ptr, 0,Array_size);
        return result;
    }
    return NULL;
    
}

void *my_realloc(void *ptr, size_t size) 
{
    if(ptr == NULL) return my_malloc(size);
    if(size == 0) 
    {
        my_free(ptr);
        return NULL;
    }
    
    size = (size + 7) & ~7;
    
    block_meta *block = (block_meta*)ptr - 1;
    
    if (block->magic != 0x12345678) return NULL;
        
    
    if(block->size >= size)
    {
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

        return ptr; 
    }
    else
    {
        void* new_ptr = my_malloc(size);
        if (new_ptr == NULL) return NULL;
        memcpy(new_ptr, ptr, block->size);
        my_free(ptr);
        return new_ptr;

    }
    
    
}
