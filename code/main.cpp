#include <iostream>
#include <string.h>
#include <cassert>
#include "test_specs.h"

using namespace std;

//simulated physical memory
static int pm[PM_CAP];
//indicates whether or not memory frame is taken (0 = free, 1 = taken)
static int bitmap[BM];
//each mask is 32 bit
static int MASK[BM];

static void initBitmask()
{
	//initialize bitmask from lsb to msb (least significant bit to most significant bit)
	memset(MASK,0,sizeof(MASK));
	for(int i = 0;i < BM;++i)
		MASK[i] = (MASK[i] | 0x01) << i;
}

static void printBits(int bmIndex,int* bitArray,int size)
{
	assert(size == BM);
	assert(bmIndex >= 0 && bmIndex < BM);
	//print from most significant bit to least significant bit
	for(int i = BM - 1;i >=0;--i)
		cout << ((bitArray[bmIndex] >> i) & 1) << " ";
	cout << endl;
}

static void printBitmap(int bmIndex)
{
	printBits(bmIndex,bitmap,BM);
}

static void printBitmask(int bmIndex)
{
	printBits(bmIndex,MASK,BM);
}

//maps from 0 to 1023 bits where each bit represents a memory frame in physical memory
static void enableFrame(int frameIndex)
{
	assert(frameIndex >= 0 && frameIndex < FRAMES);	
	int bucket_index = frameIndex / BM;
	int bit_index = frameIndex % BM;
	bitmap[bucket_index] |= MASK[bit_index];
}

static void disableFrame(int frameIndex)
{
	assert(frameIndex >= 0 && frameIndex < FRAMES);
	int bucket_index = frameIndex / BM;
	int bit_index = frameIndex % BM;
	bitmap[bucket_index] &= ~MASK[bit_index];
}

//converts bmIndex to the base frame index of physical memory
static int baseFrameIndex(int bmIndex)
{
	assert(bmIndex >= 0 && bmIndex < BM);
	return bmIndex * BM;
}

void bitmap_manip_tests()
{
	initBitmask();
	//init bitmap to be all 0s
	memset(bitmap,0,sizeof(bitmap));

	//print bitmask array
//	for(int i = 0;i < BM;++i)
//	{
//		cout << "bitmask[" << i << "] = ";
//		printBitmask(i);
//	}

	{
		printBitmap(0);
		enableFrame(0);
		printBitmap(0);
		cout << endl;
	}
	
	{
		cout << "bmIndex = 10" << endl;
		printBitmap(10);
		for(int i = baseFrameIndex(10);i < baseFrameIndex(10) + FRAME_CAP;++i)
			enableFrame(i);
		printBitmap(10);

		for(int i = baseFrameIndex(10);i < baseFrameIndex(10) + FRAME_CAP;i+=2)
			disableFrame(i);
		printBitmap(10);
	}
	
	{
		cout << "bmIndex = 31" << endl;
		printBitmap(31);
		enableFrame(baseFrameIndex(31));
		enableFrame(993);
		enableFrame(1023);
		printBitmap(31);
		disableFrame(baseFrameIndex(31));
		printBitmap(31);
	}
}

struct VirtualAddress
{
	//the following are bit fields which are used to compactly store data ~ tradeoff is that bitfields cannot
	//be accessed as a pointer because the C/C++ standard requires that data is only byte addressable (8 bits)
	unsigned int s : 9; //segment number is 9 bits
	unsigned int p : 10; //page number is 10 bits
	unsigned int w : 9; //page offset is 9 bits

	//breaks down virtual address into 3 components: s,p,w
	//least significant bit order (need to clarify if assumption wrong FIX)
	//0th to 8th bits represent w = page offset
	//9th to 18th bits represent p = page number
	//19th to 27th bits represent s = segment number
	//28th to 31th bits are unused (these are the leading bits)
	VirtualAddress(unsigned int virt_addr)
	{
		assert(virt_addr >= 0);
		s = 0,p = 0,w = 0;
		for(int i = 0;i < S;++i) 
			w |= ((virt_addr >> i) & 0x01) << i;
		for(int i = S;i < S + P;++i)
			p |= ((virt_addr >> i) & 0x01) << (i - S);
		for(int i = (S + P);i < (S + P) + W;++i)
			s |= ((virt_addr >> i) & 0x01) << (i - (S + P));
	}
};

int main(int argc,char* argv[])
{
//TODO: be able to breakdown a Virtual Address into its 3 components: s (segment number), p (page number), w (page offset)

	int virtual_addr = 1048586;//1048576;//268435455;
	cout << virtual_addr << endl;
	VirtualAddress v(virtual_addr);

	cout << VIRTUAL_ADDRESSES << endl;

	cout << "s: " << v.s << endl;
	cout << "p: " << v.p << endl;
	cout << "w: " << v.w << endl;

	return 0;
}
