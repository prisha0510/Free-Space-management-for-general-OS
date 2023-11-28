#include "my_mmu.h"

int main(){
    initialise_heap();
    info();
    void* ptr1 = my_malloc(10000);
    info();
    void* ptr2 = my_malloc(35);
    info();
    my_free(ptr2);
    info();
    void* ptr2 = my_calloc(100,4);
    info();
    my_free(ptr1);
    info();
    return 0;
}