#include <iostream>
#include <fstream>
#include <string.h>
#include <string>
#include <sstream>
#include <cassert>
#include <vector>
#include <unordered_map>
#include <errno.h>
#include "test_specs.h"

using namespace std;

//simulated physical memory
static int PM[PM_CAP];
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

//prints from msb to lsb order (left to right)
static void printBits(int bmIndex,int* bitArray,int size)
{
	assert(size == BM);
	assert(bmIndex >= 0 && bmIndex < BM);
	//print from most significant bit to least significant bit
	for(int i = BM - 1;i >=0;--i)
		cout << ((bitArray[bmIndex] >> i) & 1);//<< " ";
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

static bool isFrameEnabled(int frameIndex)
{
	assert(frameIndex >= 0 && frameIndex < FRAMES);
	int bucket_index = frameIndex / BM;
	int bit_index = frameIndex % BM;
	return bitmap[bucket_index] & MASK[bit_index];
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
		for(int i = baseFrameIndex(10);i < baseFrameIndex(10) + BM;++i)
			enableFrame(i);
		printBitmap(10);

		//check if isFrameEnabled works ~ note: this prints from msb to lsb order (left to right)
		cout << "areFramesEnabled: " << endl;
		for(int i = baseFrameIndex(10) + BM - 1;i >= baseFrameIndex(10);--i)
		{
			cout << isFrameEnabled(i);
		}
		cout << endl;

		//disable every other frame
		for(int i = baseFrameIndex(10) + BM - 1;i >= baseFrameIndex(10);i-=2)
			disableFrame(i);

		printBitmap(10);
		cout << "areFramesEnabled: " << endl;
		for(int i = baseFrameIndex(10) + BM - 1;i >= baseFrameIndex(10);--i)
		{
			cout << isFrameEnabled(i);
		}
		cout << endl;
	}
	
	{
		cout << "bmIndex = 31" << endl;
		printBitmap(31);
		enableFrame(baseFrameIndex(31));
		enableFrame(993);
		enableFrame(1023);
		printBitmap(31);
		cout << "areFramesEnabled: " << endl;
		for(int i = baseFrameIndex(31) + BM - 1;i >= baseFrameIndex(31);--i)
		{
			cout << isFrameEnabled(i);
		}
		cout << endl;

		disableFrame(baseFrameIndex(31));
		printBitmap(31);
		cout << "areFramesEnabled: " << endl;
		for(int i = baseFrameIndex(31) + BM - 1;i >= baseFrameIndex(31);--i)
		{
			cout << isFrameEnabled(i);
		}
		cout << endl;
	}
}

struct VirtualAddress
{
	//the following are bit fields which are used to compactly store data ~ tradeoff is that bitfields cannot
	//be accessed as a pointer because the C/C++ standard requires that data is only byte addressable (8 bits)
	unsigned int s : 9; //segment number is 9 bits
	unsigned int p : 10; //page number is 10 bits
	unsigned int w : 9; //page offset is 9 bits
	unsigned int sp : 19; //segment number with page number used for TLB

	//breaks down virtual address into 3 components: s,p,w
	//least significant bit order
	//0th to 8th bits represent w = page offset
	//9th to 18th bits represent p = page number
	//19th to 27th bits represent s = segment number
	//28th to 31th bits are unused (these are the leading bits)
	VirtualAddress(unsigned int virt_addr)
	{
		assert(virt_addr >= 0 && virt_addr < (unsigned int)VIRTUAL_ADDRESSES);
		s = 0,p = 0,w = 0,sp = 0;
		for(int i = 0;i < W;++i) 
			w |= ((virt_addr >> i) & 0x01) << i;
		for(int i = W;i < W + P;++i)
			p |= ((virt_addr >> i) & 0x01) << (i - W);
		for(int i = (W + P);i < (W + P) + S;++i)
			s |= ((virt_addr >> i) & 0x01) << (i - (W + P));

		//get sp for TLB
		for(int i = W;i < (W + P + S);++i)
			sp |= ((virt_addr >> i) & 0x01) << (i - W);

	}
};

struct TLB
{
	//0 least recently used to 3 most recently used
	unsigned int lru[TLB_SIZE];
	//obtained from VirtualAddresses
	unsigned int sp[TLB_SIZE];
	//physical addresses
	unsigned int f[TLB_SIZE];
}tlb;

//returns the index ranging from 0 to 3 inclusive of the tlb
//returns -1 if supplied virtual address sp could not be found
static int findTLBIndex(unsigned int sp)
{
	for(int i = 0;i < TLB_SIZE;++i)
	{
		if(sp == tlb.sp[i])
			return i;
	}
	return -1;
}

//sp obtained from VirtualAddress struct
static void updateTLBMiss(const VirtualAddress& v)
{
	for(int tlb_index = 0;tlb_index < TLB_SIZE;++tlb_index)
	{
		if(tlb.lru[tlb_index] == 0)
		{
			tlb.lru[tlb_index] = 3;//set to mru
			tlb.sp[tlb_index] = (unsigned int)v.sp;
			tlb.f[tlb_index] = PM[PM[v.s] + v.p];//set to pa
			for(int k = 0;k < TLB_SIZE;++k)
			{
				if(tlb_index == k) continue;
				--tlb.lru[k];
			}
			break;
		}
	}
}

void virtual_constructor_test()
{
	int virtual_addr = 1048586;//1048576;//268435455;
	cout << virtual_addr << endl;
	VirtualAddress v(virtual_addr);
	cout << "s: " << v.s << endl;
	cout << "p: " << v.p << endl;
	cout << "w: " << v.w << endl;
	cout << "sp: " << v.sp << endl;
	cout << endl;

	virtual_addr = 1048576;
	cout << virtual_addr << endl;
	VirtualAddress v2(virtual_addr);
	cout << "s: " << v2.s << endl;
	cout << "p: " << v2.p << endl;
	cout << "w: " << v2.w << endl;
	cout << "sp: " << v2.sp << endl;
	cout << endl;

	virtual_addr = 268435455;
	cout << virtual_addr << endl;
	VirtualAddress v3(virtual_addr);
	cout << "s: " << v3.s << endl;
	cout << "p: " << v3.p << endl;
	cout << "w: " << v3.w << endl;
	cout << "sp: " << v3.sp << endl;
	cout << endl;

	virtual_addr = 523788;
	cout << virtual_addr << endl;
	VirtualAddress v4(virtual_addr);
	cout << "s: " << v4.s << endl;
	cout << "p: " << v4.p << endl;
	cout << "w: " << v4.w << endl;
	cout << "sp: " << v4.sp << endl;
	cout << endl;

	
}

void test_stringstreams()
{
	int values[15];
	stringstream test("2 4 6 8 10 12");
	if(test.good()) cout << "The test is good" << endl;
	for(int i = 0;!test.eof() && i < 15;++i)
	{
		test >> values[i];
	}

	for(int i = 0;i < 15;++i)
		cout << values[i] << endl;
}

//TODO: make a driver that can: ~ DONE
//1. load "simulated" ram values from a file
//1st line: PM[s] = f where s = 0th frame that ranges from 0 to 511 index values;(accesses segment table).
//2nd line: PM[PM[s] + p] gives us the value of the associated PT entry's physical memory address.(access the page table)
//e.g. 15 512 PT of segment 15 starts at physical address 512
//e.g. 7 13 4096 page 7 of segment 13 starts at physical address 4096

//2. read virtual addresses from a file and parse 0 as read and 1 as write
//e.g. o1 Va1 o2 Va2 ... oi Vai = 0 1048576 1 1048586 1 1049088
//e.g. 0 1048576 is read as "read virtual address 1048576 if it is valid (valid if virtual address is in physical memory)

int main(int argc,char* argv[])
{
	//bitmap_manip_tests();
	//virtual_constructor_test();
	//test_stringstreams();

	if(argc != 3)
	{
		cerr << "Usage: " << argv[0] << " path/to/initPM.txt path/to/virtualAddresses.txt" << endl;
		return -1;
	}
	
	//BITMASK AND BITMAP INITIALIZATION
	initBitmask();
	//init bitmap to be all 0s
	memset(bitmap,0,sizeof(bitmap));
	//enable frame 0 to be ST
	enableFrame(0);
	
	//first file initializes PM
	ifstream inputFile;
	inputFile.open(argv[1]);
	if(inputFile.fail())
	{
		cerr << "Error " << argv[1] << ": " << strerror(errno) <<  endl;
		return -1;
	}	
	if(inputFile.is_open())
	{
		//first line consists of si,fi pairs where si = segment number and fi = physical address of PT
		vector<int> seg_nums;//these arrays will be ultimately stored into PM
		vector<int> phys_addrs;
		string line;
		getline(inputFile,line);
		if(!inputFile)
		{
			cerr << "failure attempt to read line 1 of file" << endl;
			return -1;
		}
		stringstream firstline(line);
		if(!firstline.good())
		{
			cerr << "error has occurred when reading the firstline of the input file" << endl;
			return -1;
		}
		cout << "segment number | physical_address" << endl;
		while(!firstline.eof())
		{
			int seg_num, phys_addr;//check
			if(firstline >> seg_num >> phys_addr) //check if values being passed are valid
			{
				seg_nums.push_back(seg_num);
				phys_addrs.push_back(phys_addr);
				cout << seg_num << " | " << phys_addr << endl;
			}
		}

		//load in address values of PT into ST
		while(!seg_nums.empty() && !phys_addrs.empty())
		{
			int seg_num = seg_nums.back();
			int phys_addr = phys_addrs.back();
			PM[seg_num] = phys_addr;
			seg_nums.pop_back();
			phys_addrs.pop_back();
			//TODO: enable frame being set here in bitmap ~ done but check
			int frame_index = phys_addr / FRAME_CAP;
			enableFrame(frame_index); //reserve 2 consecutive frames for PT
			enableFrame(frame_index + 1);
		}


		//second line consist of pj,sj,fj triplets where pj = page number, sj = segment number and fj = physical address of page pj of segment sj.
		getline(inputFile,line);
		if(!inputFile)
		{
			cerr << "failure attempt to read line 2 of file" << endl;
			return -1;
		}
		stringstream secondline(line);
		if(!secondline.good())
		{
			cerr << "error has occurred when reading the secondline of the input file" << endl;
			return -1;
		}
		vector<int> seg_nums2;
		vector<int> page_nums2;
		vector<int> phys_addrs2;
	
		cout << "page_number | segment_number | physical_address" << endl;
		while(!secondline.eof())
		{
			int page_num, seg_num, phys_addr;
			
			if(secondline >> page_num >> seg_num >> phys_addr) //check if inputs are valid
			{
				page_nums2.push_back(page_num);
				seg_nums2.push_back(seg_num);
				phys_addrs2.push_back(phys_addr);
				cout << page_num << " " << seg_num << " " << phys_addr << endl;
			}
		}

		inputFile.close();

		//load in physical address into each page p of segment s
		while(!seg_nums2.empty() && !page_nums2.empty() && !phys_addrs2.empty())
		{
			int seg_num = seg_nums2.back();
			int page_num = page_nums2.back();
			int phys_addr = phys_addrs2.back();

			//cout << "PM[" << seg_num << "] = " << PM[seg_num] << endl;
			//cout << "page_num = " << page_num << endl;
			//cout << PM[seg_num] + page_num << endl;
			//cout << "phys_addr: " << phys_addr << endl;
			PM[PM[seg_num] + page_num] = phys_addr;	

			seg_nums2.pop_back();
			page_nums2.pop_back();
			phys_addrs2.pop_back();
		}
		
	}

	//second file reads the input file that contains virtual addresses and their supported ops 0 for read, 1 for write
	//assume: each pair is stored on one line
	ifstream inputFile2;
	inputFile2.open(argv[2]);
	if(inputFile2.fail())
	{
		cerr << "Error " << argv[2] << " : " <<  strerror(errno) << endl;
		return -1;
	}
	if(inputFile2.is_open())
	{
		string line;
		getline(inputFile2,line);
		if(!inputFile2)
		{
			cerr << "Error: failure attempt to read line 1 of virtual address file" << endl;
			return -1;
		}
		stringstream virtual_addr_input(line);
		if(!virtual_addr_input.good())
		{
			cerr << "Error: failure when reading the first line of the input file" << endl;
			return -1;
		}
		//read = false (0)
		//write = true (1)
		vector<bool> ops;//array of operations used to process on the corresponding virtual address entries
		vector<int> virt_addrs;

		cout << "operation : virtual address" << endl;
		while(!virtual_addr_input.eof()) //oops reading the last line twice here..
		{
			bool op;
			int virt_addr;
			if(virtual_addr_input >> op >> virt_addr) //check if 2 consecutive inputs are valid numeric
			{
				ops.push_back(op);
				virt_addrs.push_back(virt_addr);
				cout << op << " " << virt_addr << endl;
			}	
		}

		inputFile2.close();

		//attempt to translate VA into PA by:
		//1. breaking down each VA entry into s,p,w
		//2. TODO: read access logic ~ DONE
		//3. TODO: write access logic ~ DONE
		//4. TODO: optimize translations with TLB ~ DONE
		
		for(unsigned int i = 0;i < ops.size() && i < virt_addrs.size();++i)
		{
			bool op = ops[i];
			int virt_addr = virt_addrs[i];
			//break VA into s,p,w
			VirtualAddress v(virt_addr);

			if(!op) // read
			{
				//note: counts as a tlb miss if page fault or page table does not exist
				cout << "read:" << endl;
				if(PM[v.s] == -1 || PM[PM[v.s] + v.p] == -1)
				{
					cout << "m pf" << endl;
				}
				else if(PM[v.s] == 0 || PM[PM[v.s] + v.p] == 0)
				{
					cout << "m err" << endl;
				}
				else
				{
					//locate resolved VA in tlb using sp
					int tlbIndex = findTLBIndex((unsigned int)v.sp);
					bool isTLBHit = tlbIndex != -1;
					if(isTLBHit)
					{
						cout << "read tlb hit" << endl;
						cout << "read pa: " << tlb.f[i] + v.w << endl;
						tlb.lru[i] = 3;//set to most recently used
						//decrement all lru values besides i by 1
						for(int k = 0;k < TLB_SIZE;++k)
						{
							if(tlbIndex == k) continue;
							--tlb.lru[k];
						}
					}

					if(!isTLBHit) // tlb miss
					{
						cout << "read tlb miss" << endl;
						cout << "read physical_address: " << PM[PM[v.s] + v.p] + v.w << endl;
						//find lru[i] == 0 and set this lru to 3 (evict least recently used value)
						updateTLBMiss(v);
					//	for(int tlb_index = 0;tlb_index < TLB_SIZE;++tlb_index)
					//	{
					//		if(tlb.lru[tlb_index] == 0)
					//		{
					//			tlb.lru[tlb_index] = 3;//set to mru
					//			tlb.sp[tlb_index] = (unsigned int)v.sp;
					//			tlb.f[tlb_index] = PM[PM[v.s] + v.p];//set to pa
					//			for(int k = 0;k < TLB_SIZE;++k)
					//			{
					//				if(tlb_index == k) continue;
					//				--tlb.lru[k];
					//			}
					//			break;
					//		}
					//	}
					}
				}
			}
			else //write
			{
				cout << "write:" << endl;
				if(PM[v.s] == -1 || PM[PM[v.s] + v.p] == -1)
				{	//note: counts as tlb miss if write op cannot find PA
				//	cout << "v.s = " << v.s << endl;
				//	cout << "PM[" << v.s << "] = " << PM[v.s] << endl;
				//	cout << "v.p = " << v.p << endl;
				//	cout << "PM[PM[" << v.s << "] + " << v.p << "] = " << endl;
					cout << "m pf" << endl;
				}
				else if(PM[v.s] == 0)
				{
					//allocate new blank PT (all zeroes)
					//locate new physical address of PT that contains all zeroes in 2 consecutive frames
					cout << "creating an new blank PT" << endl;
					bool canFindFreeFrame = false;
					for(int frame_index = 0;frame_index < FRAMES - 1;++frame_index)
					{
						if(!isFrameEnabled(frame_index) && !isFrameEnabled(frame_index + 1))
						{
							canFindFreeFrame = true;
							//update each frame to be enabled in the bitmap enabled
							enableFrame(frame_index);
							enableFrame(frame_index + 1);
							//update ST entry ~ convert base frame index i to PA
							PM[v.s] = frame_index * FRAME_CAP;
							break;
						}
					}

					if(!canFindFreeFrame)
					{
						cout << "error: cannot find 2 frames to allocate to" << endl;
					}
					//continue with translation process
				}
				else if(PM[PM[v.s] + v.p] == 0)
				{
					//create a new blank page
					//update the PT entry ~ locate new physical address of PT entry
					cout << "creating a new blank PT entry" << endl;
					bool canFindFreeFrame = false;
					for(int frame_index = 0;frame_index < FRAMES;++frame_index)
					{
						if(!isFrameEnabled(frame_index))
						{
							canFindFreeFrame = true;
							enableFrame(frame_index);
							PM[PM[v.s] + v.p] = frame_index * FRAME_CAP;
							break;
						}
					}

					if(!canFindFreeFrame)
					{
						cout << "error: cannot find 1 frame to allocate to" << endl;
					}
					//continue with translation process
				}
				else
				{
					//locate resolved VA in tlb using sp
					int tlbIndex = findTLBIndex((unsigned int)v.sp);
					bool isTLBHit = tlbIndex != -1;
					if(isTLBHit)
					{
						cout << "write tlb hit" << endl;
						cout << "write pa: " << tlb.f[tlbIndex] + v.w << endl;
						tlb.lru[i] = 3;//set to most recently used
						//decrement all lru values besides i by 1
						for(int k = 0;k < TLB_SIZE;++k)
						{
							if(tlbIndex == k) continue;
							--tlb.lru[k];
						}
					}

					if(!isTLBHit)
					{
						cout << "write tlb miss" << endl;
						cout << "write physical_addresss: " << PM[PM[v.s] + v.p] + v.w << endl;
						updateTLBMiss(v);
					}
				}
			}
		}
	}


	//validate here that input file 1 loads in data into PM successfully
	for(int i = 0;i < PM_CAP;++i)
	{
		if(PM[i] == 0) continue;
		cout << "PM[" << i << "] = " << PM[i] << endl;
	}
	return 0;
}
