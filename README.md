##first edit
# Free-Space-management-for-general-OS
Implementing own library my_mmu.h, replacing the malloc/calloc, realloc and free functions available in the C library for memory management.

The functions my_malloc, my_calloc, my_free  have the same calling conventions as the original malloc, calloc, and realloc in C. The file my_mmu.h contains the implementation of these functions. It also contains a function info(), a call to this function prints information about the current state of the heap. 

The program can be tested by running the test.c file (user can modify this file to test different functions and test cases)
