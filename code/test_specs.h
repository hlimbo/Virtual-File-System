#ifndef TEST_SPECS_H
#define TEST_SPECS_H
#include <cmath>

//Defines the hard limits of the simulated physical memory (RAM)

//physical memory capacity measured in ints ~ e.g. 2 MB
#define PM_CAP (524288)
//number of physical addresses available in physical memory ~ alias for PM_CAP
#define PHYSICAL_ADDRESSES (PM_CAP)
//frame capacity measured in ints ~ defines many integers are in 1 frame of memory
#define FRAME_CAP (512)

//page table size measured in frames 
#define PT_SIZE (2)
//segment table size measured in frames
#define ST_SIZE (1)
//program/data page size measured in frames
#define PROG_SIZE (1)

//total number of frames ~ e.g. 1024 frames
#define FRAMES ((PM_CAP) / (FRAME_CAP))
//total number of page tables ~ e.g. 512 page tables
#define PAGE_TABLES ((FRAMES) / (PT_SIZE))

//Segment number size measured in bits
#define S (9)
//Page number size measured in bits
#define P (10)
//Offset within Page size measured in bits
#define W (9)
//Virtual Address size measured in bits ~ 32 bits (4 bits are left unused)
#define VA ((S) + (P) + (W) + (4))
//Physical Address size defined in bits
#define PA (19)
//bitmap size
#define BM (32)

//total number of virtual addresses 2^28 = 268435456
const int VIRTUAL_ADDRESSES = std::pow(2,S + P + W);
#endif
