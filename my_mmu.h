#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/mman.h>
#include <string.h>
#include <limits.h>

typedef struct {
    int size;
    long long int magic;
} DataNodeHeader;

typedef struct FreelistHeader* FreeNodePointer;

struct FreelistHeader{
    int free_size;
    struct FreelistHeader *next;
};

// Header for each memory block allocated has a size of 16B
// We start by initialising a heap of 4 KB using mmap
// aligned to 16B (memory allocated in sizes of 16B each, like the original malloc())

int datanode_header_size = sizeof(DataNodeHeader);
int freenode_header_size = sizeof(struct FreelistHeader);
int curr_max_heap_size = 4096;

void* heap_start;
FreeNodePointer freelist_start;

int heap_size;
int num_blocks;

void* allocate_memory(size_t size){
    return mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0);
}

void initialise_heap(){
    heap_start = allocate_memory(curr_max_heap_size);
    if(heap_start==MAP_FAILED){
        printf("Error initialising heap!\n");
        return;
    }
    
    freelist_start = (FreeNodePointer)heap_start;
    freelist_start->free_size = curr_max_heap_size - freenode_header_size; 
	freelist_start->next = NULL;

    heap_size = freenode_header_size;
    num_blocks = 0;
    return;
}

// Function to allocate memory using mmap
// Uses First Fit strategy

void expand_heap(){
    // Increase the heap size
    void* second_half_start =allocate_memory(curr_max_heap_size);
    if(second_half_start==MAP_FAILED){
        printf("Error initialising heap!\n");
        return;
    }
    memcpy((void*)heap_start+curr_max_heap_size, second_half_start, curr_max_heap_size);

    // Increase the last of the last free node
    FreeNodePointer curr_node = freelist_start;
    FreeNodePointer prev_node = NULL;
    while(curr_node!=NULL){
        prev_node = curr_node;
        curr_node = curr_node->next;
    }
    if(prev_node!=NULL)
        prev_node->free_size += curr_max_heap_size;
    else
        printf("Error allocating memory!\n");

    // heap size is doubled upon request of more memory
    curr_max_heap_size*=2;
}

void* my_malloc(size_t size) {
    int rem_size = size%16;
    if(rem_size)
        size+=16-rem_size;
    int total_size = size+datanode_header_size;
    FreeNodePointer curr_node = freelist_start;
    FreeNodePointer prev_node = NULL;
    while(curr_node!=NULL && curr_node->free_size < total_size){
        prev_node = curr_node;
        curr_node = curr_node->next;
    }
    if(curr_node==NULL){
        // no space avaliable in the current heap
        expand_heap();
        return my_malloc(size);
    }
    else{
        //space can be allocated in the current heap
        num_blocks++;
        heap_size+=total_size;
        curr_node->free_size-=total_size;
        // shifting FreeNode ahead
        memcpy((void*)curr_node+total_size, curr_node, sizeof(*curr_node));
        if(curr_node==freelist_start){
            freelist_start=(FreeNodePointer)((void*)freelist_start + total_size);
        }
        
        DataNodeHeader* data = (DataNodeHeader*)curr_node;
        data->size = size;
        data->magic = 1234567;   
        curr_node= (FreeNodePointer)((void*)curr_node + total_size);
        if(prev_node!=NULL)
            prev_node->next = curr_node;
        return curr_node-(size/16);
    }
}

// Function to allocate and initialize memory to zero using mmap
void* my_calloc(size_t nelem, size_t size) {
    int rem_size = size%16;
    if(rem_size)
        size+=16-rem_size;
    void* ptr = my_malloc(nelem * size);
    if (ptr != NULL)
        memset(ptr, 0, nelem * size);
    return ptr;
}

void insert(FreeNodePointer FreeNode){
    // adding the FreeNode to the FreeList
    FreeNodePointer curr_node = freelist_start;
    FreeNodePointer prev_node = NULL;
    while(curr_node!=NULL && curr_node<FreeNode){
        prev_node = curr_node;
        curr_node = curr_node->next;
    }
    if(curr_node!=NULL){
        if(prev_node==NULL)     
            freelist_start = FreeNode;
        else                    
            prev_node->next = FreeNode;
        FreeNode->next = curr_node;
    }
    else{
        prev_node->next = FreeNode;
        FreeNode->next = NULL;
    }

    //merging freenodes
    if(prev_node!=NULL){
        if((void*)prev_node+prev_node->free_size+freenode_header_size==(void*)FreeNode){
            prev_node->free_size += freenode_header_size+ FreeNode->free_size;
            prev_node->next = FreeNode->next;
            FreeNode = prev_node;
            heap_size-=freenode_header_size;
        }
    }
    if(FreeNode->next!=NULL){
        if((void*)FreeNode+freenode_header_size+ FreeNode->free_size == (void*)FreeNode->next){
            FreeNode->free_size += freenode_header_size + FreeNode->next->free_size;
            FreeNode->next = FreeNode->next->next;
            heap_size-=freenode_header_size;
        }
    }
}

// Function to release memory allocated using my_malloc and my_calloc
void my_free(void* ptr) {
    DataNodeHeader* data_header = (DataNodeHeader*)ptr;
    data_header--;
    // data_header points to the header of the allocated region
    int delta_size = data_header->size + datanode_header_size - freenode_header_size;
    // delta_size is the change in size
    FreeNodePointer FreeNode = (FreeNodePointer)data_header;
    FreeNode->free_size =delta_size;
    // FreeNode is the new node that takes the place of the previously allocated region.
    num_blocks--;
    heap_size-=delta_size;
    insert(FreeNode);
}

void* my_realloc(void* ptr, size_t size){
    if(ptr==NULL)
        return my_malloc(size);
    else if (size == 0) {
        my_free(ptr);
        return NULL;
    } 
    else{
        int rem_size = size%16;
        if(rem_size)
            size+=16-rem_size;
        DataNodeHeader* data_header = (DataNodeHeader*)ptr;
        data_header--;
        int delta = size - data_header->size;
        FreeNodePointer next_free_space = (FreeNodePointer)((void*)ptr + data_header->size);
        FreeNodePointer curr_node = freelist_start;
        FreeNodePointer prev_node = NULL;
        if(delta>0){
            while(curr_node!=NULL){
                if(curr_node==next_free_space)
                    break;
                prev_node = curr_node;
                curr_node = curr_node->next;
            }
            if(curr_node!=NULL){
                if(next_free_space->free_size >= delta){
                    prev_node->next = (FreeNodePointer)((void*)curr_node + delta);
                    curr_node->free_size-=delta;
                    data_header->size+=delta;
                    heap_size+=delta;
                    return ptr;
                }
                else{
                    // The current block cannot be resized in place; allocate a new block.
                    void* new_ptr = my_malloc(size);
                    if (new_ptr == NULL) {
                        printf("Error initialising memory!\n");
                        return NULL;
                    }
                    memcpy(new_ptr, ptr, data_header->size);
                    my_free(ptr);
                    return new_ptr;
                }
            }
            else{
                // The current block has another data node in front of it.
                void* new_ptr = my_malloc(size);
                if (new_ptr == NULL) {
                    printf("Error initialising memory!\n");
                    return NULL;
                }
                memcpy(new_ptr, ptr, data_header->size);
                my_free(ptr);
                return new_ptr;
            }
        }
        else{
            // Just need to free the last delta bytes of the allocated region
            delta = -delta;
            data_header->size-=delta;
            FreeNodePointer FreeNode = (FreeNodePointer)((void*)ptr + data_header->size - delta);
            FreeNode->free_size = delta-freenode_header_size;
            insert(FreeNode);
            heap_size-=delta;
            return ptr;
        }
        
    }
}

void info(){
    fprintf(stderr, "Starting address of heap = %p\n", heap_start);
    fprintf(stderr, "Current allocated space in heap = %d\n", heap_size);
    fprintf(stderr, "Starting address of free space = %p\n", freelist_start);
    fprintf(stderr, "Number of allocated regions = %d\n", num_blocks);
    FreeNodePointer curr_node = freelist_start;
    int node_num = 1;
    while(curr_node!=NULL){
        fprintf(stderr, "Size of node %d in freelist is %d\n",node_num, curr_node->free_size);
        node_num++;
        curr_node = curr_node->next;
    }
    fprintf(stderr, "\n");
}



