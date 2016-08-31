//
//  COMP1927 Assignment 1 - Vlad: the memory allocator
//  allocator.c ... implementation
//
//  Created by Liam O'Connor on 18/07/12.
//  Modified by John Shepherd in August 2014, August 2015
//  Copyright (c) 2012-2015 UNSW. All rights reserved.
//
//  Modified by Clinton Hadinata in August 2015
//  COMP1927 2015 Semester 2
//  Tutor: Oliver Tan, tue12-tuba
//
//  Note: printf statements used to debug have been commented
//        out and indented.
//

#include "allocator.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define HEADER_SIZE    sizeof(struct free_list_header)  
#define MAGIC_FREE     0xDEADBEEF
#define MAGIC_ALLOC    0xBEEFDEAD

typedef unsigned char byte;
typedef u_int32_t vlink_t;
typedef u_int32_t vsize_t;
typedef u_int32_t vaddr_t;

typedef struct free_list_header {
   u_int32_t magic;  // ought to contain MAGIC_FREE
   vsize_t size;     // # bytes in this block (including header)
   vlink_t next;     // memory[] index of next free block
   vlink_t prev;     // memory[] index of previous free block
} free_header_t;

// Global data

static byte *memory = NULL;   // pointer to start of allocator memory
static vaddr_t free_list_ptr; // index in memory[] of first block in free list
static vsize_t memory_size;   // number of bytes malloc'd in memory[]


// Miscellaneous functions prototypes:

// Function to test if x is a power of two.
int isPowerOfTwo (u_int32_t x);

// Function that takes in an integer and returns 
// the closest power of two that is higher than it.
u_int32_t whatPowerUp(int n);

// Function that takes in an integer and returns 
// the closest power of two that is lower than it.
u_int32_t whatPowerDown(int n);

// Function that returns the memory[] index of a given pointer to a header.
vlink_t whatIndex(free_header_t *ptr);

// Function that return the address of a given memory[] index.
free_header_t *whatAddress(vaddr_t index);

// Function that splits the block of memory ptr is pointing to
// into two equal-sized blocks, returning a pointer to the new block
// at the halfway point.
free_header_t *insertHalve(free_header_t *ptr); 

// Determines if new block to be allocated 
// is correct size for allocating n bytes
int sizeOK (free_header_t *header, u_int32_t n);

// Determines if memory block is a valid FREE block;
int magicFreeOK(free_header_t *header);

// Determines if memory block is a valid ALLOCATED block;
int magicAllocOK(free_header_t *header);

// Determines if a block of memory pointed to by ptr
// can be merged with the block NEXT to it.
int canMergeNext(free_header_t *ptr);

// Determines if a block of memory at index with size = size was
// split from the region consisting it and the block NEXT to it.
int wasSplit(vlink_t index, vsize_t size);

// Merges the block of memory with the block NEXT to it.
void merge_with_next(free_header_t *ptr);



// Input: size - number of bytes to make available to the allocator
// Output: none              
// Precondition: Size is a power of two.
// Postcondition: `size` bytes are now available to the allocator
// 
// (If the allocator is already initialised, this function does nothing,
//  even if it was initialised with different size)

void vlad_init(u_int32_t size)
{
   if (!isPowerOfTwo(size)) {
      size = whatPowerUp(size);
   }

   memory_size = size;
   memory = malloc(memory_size);

      //printf("memory_size is %d\n", size);

   free_header_t *header = (free_header_t *) memory;

   header->magic = MAGIC_FREE;
   header->size = size;
   header->next = 0;
   header->prev = 0;

   free_list_ptr = 0;
}


// Input: n - number of bytes requested
// Output: p - a pointer, or NULL
// Precondition: n is < size of memory available to the allocator
// Postcondition: If a region of size n or greater cannot be found, p = NULL 
//                Else, p points to a location immediately after a header block
//                      for a newly-allocated region of some size >= 
//                      n + header size.

void *vlad_malloc(u_int32_t n)
{
   free_header_t *ptr = (free_header_t *)(memory + free_list_ptr);
      // printf("ptr is at %p index %d\n\n", ptr, free_list_ptr);

   // Trawler trawls through the free list to find ANY memory chunk
   // that n would fit into.
   free_header_t *trawler = whatAddress(free_list_ptr);
   int teller = 0;
   do {
      if (trawler->size >= n + HEADER_SIZE) {
         teller++;
         break;
      }
      trawler = whatAddress(trawler->next);
   } while (trawler != whatAddress(free_list_ptr));

   // If no chunks are found that is big enough for n, return NULL.
   if (teller == 0) { 
      return NULL;
   }

         //printf("at least one chunk found!\n");

   // Check if there is only one free chunk, and if so,
   // if it can't be split before allocating n - (i.e. if it'll 
   // result in no free chunks being available), return NULL.
   if (trawler->next == whatIndex(trawler)  
      && trawler->prev == whatIndex(trawler)
      && trawler->size < 2*(n + HEADER_SIZE)) {
      return NULL;
   }

   // Trawler now trawls through the free list to find the smallest 
   // memory chunk that n would fit into.

   vsize_t minSize = trawler->size;
   vaddr_t minIndex = whatIndex(trawler);

   free_list_ptr = whatIndex(trawler);

   do {
      if (trawler->size > n + HEADER_SIZE && trawler->size < minSize) {
         minSize = trawler->size;
         minIndex = whatIndex(trawler);
      }
      trawler = whatAddress(trawler->next);
   } while(trawler != whatAddress(free_list_ptr));

         //printf("minSize is %d at %d\n", minSize, minIndex);


   // Once smallest memory chunk is found, split it up until (sizeOK == TRUE)
   ptr = whatAddress(minIndex);

   free_header_t *newHeader = ptr;
   newHeader->magic = newHeader->magic; // To keep compiler happy.

   while (!sizeOK(ptr,n)) {   
      newHeader = insertHalve(ptr);
   }

   assert(sizeOK(ptr, n));
         //printf("passed assert sizeOK!\n");

      // Test printfs to show the chunk of memory to be allocated:
         //printf("Memory is to be allocated at ptr.\n");
         //printf("ptr is at %d\n", whatIndex(ptr));
         //printf("ptr->size is %d\n", ptr->size);
         //printf("ptr->next = %d\n", ptr->next);
         //printf("ptr->prev = %d\n", ptr->prev);


   // Ensure the block to be allocated is free:
   if (!magicFreeOK(ptr)) {
      fprintf(stderr, "Attempt to allocate non-free memory");
      abort();
   } 

   // Set header magic to MAGIC_ALLOC
   ptr->magic = MAGIC_ALLOC;

   // Link the surrounding free blocks together, skipping
   // over the current memory block to be allocated (ptr):
   free_header_t *ptrNext = whatAddress(ptr->next);
   free_header_t *ptrPrev = whatAddress(ptr->prev);

      //printf("ptrNext is at %d\n", whatIndex(ptrNext));
      //printf("ptrPrev is at %d\n", whatIndex(ptrPrev));

   ptrNext->prev = ptr->prev; 
   ptrPrev->next = whatIndex(ptrNext); 

      //printf("1. ptrNext->prev = %d\n", ptrNext->prev);
      //printf("2. ptrPrev->next = %d\n", ptrPrev->next);


   // Set free_list_ptr to next header
   free_list_ptr = ptr->next;

      //printf("free_list_ptr now at: %d at %p\n", free_list_ptr, &memory[free_list_ptr]);
      //printf("memory returned is at: %p\n", (void*)ptr + HEADER_SIZE);
   

   return ((void*)ptr + HEADER_SIZE);
}


// Input: object, a pointer.
// Output: none
// Precondition: object points to a location immediately after a header block
//               within the allocator's memory.
// Postcondition: The region pointed to by object can be re-allocated by 
//                vlad_malloc

void vlad_free(void *object)
{
   free_header_t *ptr = (free_header_t *)(object - HEADER_SIZE);

   // Check that block to be freed is valid:
   if (!magicAllocOK(ptr)) {
      fprintf(stderr, "Attempt to free non-allocated memory");
      abort();
   } 

   // Set header magic to free:
   ptr->magic = MAGIC_FREE;

   // Find highest and lowest indices/positions in the free list:
   free_header_t *trawler = (free_header_t *)(memory + free_list_ptr);
   
   vaddr_t min = free_list_ptr;
   vaddr_t max = free_list_ptr;

   do {
      if (whatIndex(trawler) < min) {
         min = whatIndex(trawler);
      }
      if (whatIndex(trawler) > max) {
         max = whatIndex(trawler);
      }
      trawler = whatAddress(trawler->next);

   } while (trawler != whatAddress(free_list_ptr));

         //printf("min is at: %d\n", min); 


   // Declare pointers for headers above and below 
   // the newly-freed-block in free list:
   free_header_t *ptrPrev = NULL;
   free_header_t *ptrNext = NULL;

   // Now find the correct position for newly freed block:
   if (whatIndex(ptr) < min || whatIndex(ptr) > max) {
      ptrPrev = whatAddress(max);
      ptrNext = whatAddress(min);
      if (whatIndex(ptr) < min) {
         min = whatIndex(ptr);
      }
   } else {
      trawler = whatAddress(min);
      do {
         if (whatIndex(trawler) < whatIndex(ptr) 
            && trawler->next > whatIndex(ptr)) {
            ptrPrev = trawler;
            ptrNext = whatAddress(trawler->next);
            break;
         }
         trawler = whatAddress(trawler->next);
      } while (trawler != whatAddress(min));
   }

         //printf("now min is at: %d\n", min);

      // Test printfs to show the chunk of memory to be freed:
         //printf("ptr is at %d\n", whatIndex(ptr));
         //printf("ptrNext is at %d\n", whatIndex(ptrNext));
         //printf("ptrPrev is at %d\n", whatIndex(ptrPrev));

   // Assign prev and next of newly-freed-block to surrounding headers
   assert(ptrPrev != NULL && ptrNext != NULL);
   ptr->next = whatIndex(ptrNext);
   ptr->prev = whatIndex(ptrPrev);

   // Adjust next/prev of blocks above and below newly-freed-block
   // to link it to the rest of the list
   ptrNext->prev = whatIndex(ptr);
   ptrPrev->next = whatIndex(ptr);

         //printf("Now ptrNext->prev is %d\n", ptrNext->prev);
         //printf("Now ptrPrev->next is %d\n", ptrPrev->next);


   // End of freeing, now to merge..



   // Merging begins here:

   // The trawling will begin from lowest-positioned block of free memory.
   free_list_ptr = min;
   trawler = whatAddress(free_list_ptr);

   // Bullion hasMerged to check if a merge has occured within the inner loop.
   // If it goes through inner loop without any merges, the outer loop exits.
   int hasMerged = 1;
   do {
      hasMerged = 0;
      do {
         //printf("Checking mergability at %d with %d, size is:%d\n", whatIndex(trawler), trawler->next, trawler->size);
         if(canMergeNext(trawler)) {
            merge_with_next(trawler);
            //printf("Merged at %d with %d, size is:%d\n", whatIndex(trawler), trawler->next, trawler->size);
            hasMerged = 1;
         }
         trawler = whatAddress(trawler->next);
      } while (trawler != whatAddress(free_list_ptr));
   }  while (hasMerged);
   
}


// Stop the allocator, so that it can be init'ed again:
// Precondition: allocator memory was once allocated by vlad_init()
// Postcondition: allocator is unusable until vlad_int() executed again

void vlad_end(void)
{
   free(memory);
}


// Precondition: allocator has been vlad_init()'d
// Postcondition: allocator stats displayed on stdout

void vlad_stats(void)
{
   // This simply prints out all of the free blocks of memory in
   // the free list and lists their details/nodes.

   free_header_t *ptr = (free_header_t *)(memory + free_list_ptr);
   ptr = whatAddress(ptr->next);

   printf("-----------------------\n");
   printf("free_list_ptr at [%d]\n", free_list_ptr);
   printf("-----------------------\n");
   while (ptr != (free_header_t *)(memory + free_list_ptr)) {
      printf("Free Slot at [%d]\n", whatIndex(ptr));
      printf("  size = %d\n", ptr->size);
      printf("  next = %d\n", ptr->next);
      printf("  prev = %d\n", ptr->prev);
      //printf("\n");
      printf("-----------------------\n");
      //printf("\n");
      ptr = (free_header_t *)(memory + ptr->next);
   }

   printf("Last Free Slot at [%d]\n", free_list_ptr);
   printf("  size = %d\n", ptr->size);
   printf("  next = %d\n", ptr->next);
   printf("  prev = %d\n", ptr->prev);
   printf("-----------------------\n");

   return;
}


// Miscellaneous functions below:

// Function to test if x is a power of two.
int isPowerOfTwo (u_int32_t x) {
  return ((x != 0) && !(x & (x - 1)));
}

// Function that takes in an integer and returns 
// the closest power of two that is higher than it.
u_int32_t whatPowerUp(int n) {
   int power = 0;
   double f = n;
      while (f > 1) {
         f = f / 2;
         power++;
      }
   int result = 1;
   while (power != 0) {
      result = result*2;
      power--;
   }
   return result;
}

// Function that returns the memory[] index of a given pointer to a header.
vlink_t whatIndex(free_header_t *ptr) {
   vlink_t index = (vlink_t)((byte*)ptr - memory);
   return index;
}

// Function that return the address of a given memory[] index.
free_header_t *whatAddress(vaddr_t index) {
   free_header_t *ptr = (free_header_t *)(memory + index);
   return ptr;
}

// Function that splits the block of memory ptr is pointing to
// into two equal-sized blocks, returning a pointer to the new block
// at the halfway point.
free_header_t *insertHalve(free_header_t *ptr) {

   free_header_t *newHeader = (free_header_t*)((byte*)ptr+(ptr->size/2));

   newHeader->magic = MAGIC_FREE;
   newHeader->size = ptr->size/2;
   newHeader->next = ptr->next;
   newHeader->prev = whatIndex(ptr);

         //printf("newHeader size is: %d at %p\n", newHeader->size, newHeader);

   // Reassign ptr header variables:
   ptr->size = ptr->size/2;
   if (ptr->next == whatIndex(ptr) && ptr->prev == whatIndex(ptr)) {
      ptr->prev = whatIndex(newHeader);
      ptr->next = whatIndex(newHeader);
   } else {
      free_header_t *headerAfter = whatAddress(newHeader->next);
      headerAfter->prev = whatIndex(newHeader);
      ptr->next = whatIndex(newHeader);
   }

   return newHeader;

}

// Function that ensures that new block to be allocated 
// is correct size for allocating n bytes
int sizeOK (free_header_t *header, u_int32_t n) {
   return ((header->size >= HEADER_SIZE + n 
      && header->size/2 < HEADER_SIZE + n));
}

// Determines if memory block is a valid FREE block;
int magicFreeOK(free_header_t *header) {
   return (header->magic == MAGIC_FREE);
}

// Determines if memory block is a valid ALLOCATED block;
int magicAllocOK(free_header_t *header) {
   return (header->magic == MAGIC_ALLOC);
}

// Determines if a block of memory pointed to by ptr
// can be merged with the block NEXT to it.
int canMergeNext(free_header_t *ptr) {
   if ( (whatIndex(ptr) == 0 || wasSplit(whatIndex(ptr), ptr->size))
      && (whatAddress(ptr->next)->size == ptr->size)
      && (ptr->next - whatIndex(ptr) == ptr->size)) {
            //printf("canMergeNext successful\n");
            //printf("ptr->next is %d\n", ptr->next);
            //printf("ptr is at %d\n", whatIndex(ptr));
            //printf("ptr->size is %d\n", ptr->size);
         return 1;
      } else {
      return 0;
   }
}

// Determines if a block of memory at index with size = size was
// split from the region consisting it and the block NEXT to it.
int wasSplit(vlink_t index, vsize_t size) {

   // Checks if index is a multiple of 2*size:
   vsize_t increment = 2*size;

   vaddr_t i = 0;
   while (i != memory_size) {
      if (index == i) {
         return 1;
      }
      i += increment;
   }
   return 0;
}

// Merges the block of memory with the block NEXT to it.
void merge_with_next(free_header_t *ptr) {

   if (free_list_ptr == ptr->next) {
      free_list_ptr = whatIndex(ptr);
   }

   ptr->next = whatAddress(ptr->next)->next;
   whatAddress(ptr->next)->prev = whatIndex(ptr);

   ptr->size = (ptr->size)*2;
}


// ----------------------  end of my implementation   -------------------------

///////////////////////////////////////////////////////////////////////////////






//
// All of the code below here was written by Alen Bou-Haidar, COMP1927 14s2
//

//
// Fancy allocator stats
// 2D diagram for your allocator.c ... implementation
// 
// Copyright (C) 2014 Alen Bou-Haidar <alencool@gmail.com>
// 
// FancyStat is free software: you can redistribute it and/or modify 
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or 
// (at your option) any later version.
// 
// FancyStat is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>


#include <string.h>

#define STAT_WIDTH  32
#define STAT_HEIGHT 16
#define BG_FREE      "\x1b[48;5;35m" 
#define BG_ALLOC     "\x1b[48;5;39m"
#define FG_FREE      "\x1b[38;5;35m" 
#define FG_ALLOC     "\x1b[38;5;39m"
#define CL_RESET     "\x1b[0m"


typedef struct point {int x, y;} point;

static point offset_to_point(int offset,  int size, int is_end);
static void fill_block(char graph[STAT_HEIGHT][STAT_WIDTH][20], 
                        int offset, char * label);



// Print fancy 2D view of memory
// Note, This is limited to memory_sizes of under 16MB
void vlad_reveal(void *alpha[26])
{
    int i, j;
    vlink_t offset;
    char graph[STAT_HEIGHT][STAT_WIDTH][20];
    char free_sizes[26][32];
    char alloc_sizes[26][32];
    char label[3]; // letters for used memory, numbers for free memory
    int free_count, alloc_count, max_count;
    free_header_t * block;

	// TODO
	// REMOVE these statements when your vlad_malloc() is done
    //printf("vlad_reveal() won't work until vlad_malloc() works\n");
    //return;

    // initilise size lists
    for (i=0; i<26; i++) {
        free_sizes[i][0]= '\0';
        alloc_sizes[i][0]= '\0';
    }

    // Fill graph with free memory
    offset = 0;
    i = 1;
    free_count = 0;
    while (offset < memory_size){
        block = (free_header_t *)(memory + offset);
        if (block->magic == MAGIC_FREE) {
            snprintf(free_sizes[free_count++], 32, 
                "%d) %d bytes", i, block->size);
            snprintf(label, 3, "%d", i++);
            fill_block(graph, offset,label);
        }
        offset += block->size;
    }

    // Fill graph with allocated memory
    alloc_count = 0;
    for (i=0; i<26; i++) {
        if (alpha[i] != NULL) {
            offset = ((byte *) alpha[i] - (byte *) memory) - HEADER_SIZE;
            block = (free_header_t *)(memory + offset);
            snprintf(alloc_sizes[alloc_count++], 32, 
                "%c) %d bytes", 'a' + i, block->size);
            snprintf(label, 3, "%c", 'a' + i);
            fill_block(graph, offset,label);
        }
    }

    // Print all the memory!
    for (i=0; i<STAT_HEIGHT; i++) {
        for (j=0; j<STAT_WIDTH; j++) {
            printf("%s", graph[i][j]);
        }
        printf("\n");
    }

    //Print table of sizes
    max_count = (free_count > alloc_count)? free_count: alloc_count;
    printf(FG_FREE"%-32s"CL_RESET, "Free");
    if (alloc_count > 0){
        printf(FG_ALLOC"%s\n"CL_RESET, "Allocated");
    } else {
        printf("\n");
    }
    for (i=0; i<max_count;i++) {
        printf("%-32s%s\n", free_sizes[i], alloc_sizes[i]);
    }

}

// Fill block area
static void fill_block(char graph[STAT_HEIGHT][STAT_WIDTH][20], 
                        int offset, char * label)
{
    point start, end;
    free_header_t * block;
    char * color;
    char text[3];
    block = (free_header_t *)(memory + offset);
    start = offset_to_point(offset, memory_size, 0);
    end = offset_to_point(offset + block->size, memory_size, 1);
    color = (block->magic == MAGIC_FREE) ? BG_FREE: BG_ALLOC;

    int x, y;
    for (y=start.y; y < end.y; y++) {
        for (x=start.x; x < end.x; x++) {
            if (x == start.x && y == start.y) {
                // draw top left corner
                snprintf(text, 3, "|%s", label);
            } else if (x == start.x && y == end.y - 1) {
                // draw bottom left corner
                snprintf(text, 3, "|_");
            } else if (y == end.y - 1) {
                // draw bottom border
                snprintf(text, 3, "__");
            } else if (x == start.x) {
                // draw left border
                snprintf(text, 3, "| ");
            } else {
                snprintf(text, 3, "  ");
            }
            sprintf(graph[y][x], "%s%s"CL_RESET, color, text);            
        }
    }
}

// Converts offset to coordinate
static point offset_to_point(int offset,  int size, int is_end)
{
    int pot[2] = {STAT_WIDTH, STAT_HEIGHT}; // potential XY
    int crd[2] = {0};                       // coordinates
    int sign = 1;                           // Adding/Subtracting
    int inY = 0;                            // which axis context
    int curr = size >> 1;                   // first bit to check
    point c;                                // final coordinate
    if (is_end) {
        offset = size - offset;
        crd[0] = STAT_WIDTH;
        crd[1] = STAT_HEIGHT;
        sign = -1;
    }
    while (curr) {
        pot[inY] >>= 1;
        if (curr & offset) {
            crd[inY] += pot[inY]*sign; 
        }
        inY = !inY; // flip which axis to look at
        curr >>= 1; // shift to the right to advance
    }
    c.x = crd[0];
    c.y = crd[1];
    return c;
}
