// filehdr.cc
//	Routines for managing the disk file header (in UNIX, this
//	would be called the i-node).
//
//	The file header is used to locate where on disk the
//	file's data is stored.  We implement this as a fixed size
//	table of pointers -- each entry in the table points to the
//	disk sector containing that portion of the file data
//	(in other words, there are no indirect or doubly indirect
//	blocks). The table size is chosen so that the file header
//	will be just big enough to fit in one disk sector,
//
//      Unlike in a real system, we do not keep track of file permissions,
//	ownership, last modification date, etc., in the file header.
//
//	A file header can be initialized in two ways:
//	   for a new file, by modifying the in-memory data structure
//	     to point to the newly allocated data blocks
//	   for a file already on disk, by reading the file header from disk
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "filehdr.h"
#include "debug.h"
#include "synchdisk.h"
#include "main.h"

//----------------------------------------------------------------------
// MP4 mod tag
// FileHeader::FileHeader
//	There is no need to initialize a fileheader,
//	since all the information should be initialized by Allocate or FetchFrom.
//	The purpose of this function is to keep valgrind happy.
//----------------------------------------------------------------------
FileHeader::FileHeader()
{
	numBytes = -1;
	numSectors = -1;
	memset(dataSectors, -1, sizeof(dataSectors));
}

//----------------------------------------------------------------------
// MP4 mod tag
// FileHeader::~FileHeader
//	Currently, there is not need to do anything in destructor function.
//	However, if you decide to add some "in-core" data in header
//	Always remember to deallocate their space or you will leak memory
//----------------------------------------------------------------------
FileHeader::~FileHeader()
{
	// nothing to do now
}

//----------------------------------------------------------------------
// FileHeader::Allocate
// 	Initialize a fresh file header for a newly created file.
//	Allocate data blocks for the file out of the map of free disk blocks.
//	Return FALSE if there are not enough free blocks to accomodate
//	the new file.
//
//	"freeMap" is the bit map of free disk sectors
//	"fileSize" is the bit map of free disk sectors
//----------------------------------------------------------------------

// bool FileHeader::Allocate(PersistentBitmap *freeMap, int fileSize)
// {
// 	numBytes = fileSize;
// 	numSectors = divRoundUp(fileSize, SectorSize);
// 	if (freeMap->NumClear() < numSectors)
// 		return FALSE; // not enough space

// 	for (int i = 0; i < numSectors; i++)
// 	{
// 		dataSectors[i] = freeMap->FindAndSet();
// 		// since we checked that there was enough free space,
// 		// we expect this to succeed
// 		ASSERT(dataSectors[i] >= 0);
// 	}
// 	return TRUE;
// }
/*li2_3*/
void FileHeader::AllocateSubHeader(FileHeader* subHeader, PersistentBitmap* freeMap, int& fileSize, int subSize)
{
    subHeader->Allocate(freeMap, std::min(fileSize, subSize));
    fileSize -= subSize;
}
void FileHeader::CleanDiskSector(int sector)
{
    char* cleanData = new char[SectorSize]();
    kernel->synchDisk->WriteSector(sector, cleanData);
    delete[] cleanData;
}
bool FileHeader::Allocate(PersistentBitmap *freeMap, int fileSize)
{
	numBytes = fileSize;
	
	numSectors = divRoundUp(fileSize, SectorSize); 

	if (freeMap->NumClear() < numSectors)
		return FALSE;

	if(fileSize > BYTES_4LEVEL ){ 
		int i=0; 
		while(fileSize>0){	
			dataSectors[i] = freeMap->FindAndSet();
			FileHeader* subHeader = new FileHeader();
			AllocateSubHeader(subHeader, freeMap, fileSize, BYTES_4LEVEL);
			subHeader->WriteBack(dataSectors[i]);
			i++;
		}
		
	}
	if (fileSize > BYTES_3LEVEL){
		int i = 0;
		while (fileSize > 0){
			dataSectors[i] = freeMap->FindAndSet();
			FileHeader* subHeader = new FileHeader();
			AllocateSubHeader(subHeader, freeMap, fileSize, BYTES_3LEVEL);
			subHeader->WriteBack(dataSectors[i]);
			i++;
		}
	}
	else if (fileSize > BYTES_2LEVEL){
		int i = 0;
		while (fileSize > 0){
			dataSectors[i] = freeMap->FindAndSet();
			FileHeader* subHeader = new FileHeader();
			AllocateSubHeader(subHeader, freeMap, fileSize, BYTES_2LEVEL);
			subHeader->WriteBack(dataSectors[i]);
			i++;
		}
	}
	else if (fileSize > BYTES_1LEVEL){
		int i = 0;
		while (fileSize > 0){
			dataSectors[i] = freeMap->FindAndSet();
			FileHeader* subHeader = new FileHeader();
			AllocateSubHeader(subHeader, freeMap, fileSize, BYTES_1LEVEL);
			subHeader->WriteBack(dataSectors[i]);
			i++;
		}
	}
	else
	{
		for (int i = 0; i < numSectors; i++)
		{
			dataSectors[i] = freeMap->FindAndSet();
			ASSERT(dataSectors[i] >= 0); // sector number should be >= 0
			// MP4 KEY !!!!! CLEAN DISK HERE!!!
			CleanDiskSector(dataSectors[i]);
		}
	}
	return TRUE;
}

//----------------------------------------------------------------------
// FileHeader::Deallocate
// 	De-allocate all the space allocated for data blocks for this file.
//
//	"freeMap" is the bit map of free disk sectors
//----------------------------------------------------------------------

// void FileHeader::Deallocate(PersistentBitmap *freeMap)
// {
// 	for (int i = 0; i < numSectors; i++)
// 	{
// 		ASSERT(freeMap->Test((int)dataSectors[i])); // ought to be marked!
// 		freeMap->Clear((int)dataSectors[i]);
// 	}
// }
/*li2_3*/
void FileHeader::Deallocate(PersistentBitmap *freeMap)
{
    if (numBytes > BYTES_1LEVEL)
    {
        // For multi-level allocation
        for (int i = 0; i < divRoundUp(numSectors, NumDirect); i++)
        {
            FileHeader *subdir = new FileHeader();
            subdir->FetchFrom(dataSectors[i]);
            subdir->Deallocate(freeMap);
        }
    }
    else
    {
        // For direct allocation
        for (int i = 0; i < numSectors; i++)
        {
            ASSERT(freeMap->Test((int)dataSectors[i]));
            freeMap->Clear((int)dataSectors[i]);
        }
    }
}

//----------------------------------------------------------------------
// FileHeader::FetchFrom
// 	Fetch contents of file header from disk.
//
//	"sector" is the disk sector containing the file header
//----------------------------------------------------------------------

void FileHeader::FetchFrom(int sector)
{
	kernel->synchDisk->ReadSector(sector, (char *)this);

	/*
		MP4 Hint:
		After you add some in-core informations, you will need to rebuild the header's structure
	*/
}

//----------------------------------------------------------------------
// FileHeader::WriteBack
// 	Write the modified contents of the file header back to disk.
//
//	"sector" is the disk sector to contain the file header
//----------------------------------------------------------------------

void FileHeader::WriteBack(int sector)
{
	kernel->synchDisk->WriteSector(sector, (char *)this);

	/*
		MP4 Hint:
		After you add some in-core informations, you may not want to write all fields into disk.
		Use this instead:
		char buf[SectorSize];
		memcpy(buf + offset, &dataToBeWritten, sizeof(dataToBeWritten));
		...
	*/
}

//----------------------------------------------------------------------
// FileHeader::ByteToSector
// 	Return which disk sector is storing a particular byte within the file.
//      This is essentially a translation from a virtual address (the
//	offset in the file) to a physical address (the sector where the
//	data at the offset is stored).
//
//	"offset" is the location within the file of the byte in question
//----------------------------------------------------------------------

// int FileHeader::ByteToSector(int offset)
// {
// 	return (dataSectors[offset / SectorSize]);
// }
/*li2_3*/
int FileHeader::ByteToSector(int offset){
    if (numBytes > BYTES_1LEVEL){
        // For multi-level allocation
        FileHeader *subhdr = new FileHeader;
        int entry_number, level_bytes;

        if (numBytes > BYTES_4LEVEL){
            entry_number = divRoundDown(offset, BYTES_4LEVEL);
            level_bytes = BYTES_4LEVEL;
        }
        else if (numBytes > BYTES_3LEVEL){
            entry_number = divRoundDown(offset, BYTES_3LEVEL);
            level_bytes = BYTES_3LEVEL;
        }
        else if (numBytes > BYTES_2LEVEL){
            entry_number = divRoundDown(offset, BYTES_2LEVEL);
            level_bytes = BYTES_2LEVEL;
        }
        else{
            entry_number = divRoundDown(offset, BYTES_1LEVEL);
            level_bytes = BYTES_1LEVEL;
        }

        subhdr->FetchFrom(dataSectors[entry_number]);
        return subhdr->ByteToSector(offset - level_bytes * entry_number);
    }
    else{
        // For direct allocation
        return dataSectors[offset / SectorSize];
    }
}



//----------------------------------------------------------------------
// FileHeader::FileLength
// 	Return the number of bytes in the file.
//----------------------------------------------------------------------

int FileHeader::FileLength()
{
	return numBytes;
}

//----------------------------------------------------------------------
// FileHeader::Print
// 	Print the contents of the file header, and the contents of all
//	the data blocks pointed to by the file header.
//----------------------------------------------------------------------

// void FileHeader::Print()
// {
// 	int i, j, k;
// 	char *data = new char[SectorSize];

// 	printf("FileHeader contents.  File size: %d.  File blocks:\n", numBytes);
// 	for (i = 0; i < numSectors; i++)
// 		printf("%d ", dataSectors[i]);
// 	printf("\nFile contents:\n");
// 	for (i = k = 0; i < numSectors; i++)
// 	{
// 		kernel->synchDisk->ReadSector(dataSectors[i], data);
// 		for (j = 0; (j < SectorSize) && (k < numBytes); j++, k++)
// 		{
// 			if ('\040' <= data[j] && data[j] <= '\176') // isprint(data[j])
// 				printf("%c", data[j]);
// 			else
// 				printf("\\%x", (unsigned char)data[j]);
// 		}
// 		printf("\n");
// 	}
// 	delete[] data;
// }
/*li2_3*/
// void FileHeader::Print()
// {
// 	int i, j, k;
// 	char *data = new char[SectorSize];

// 	printf("FileHeader contents.  File size: %d.  File blocks:\n", numBytes);
// 	for (i = 0; i < numSectors; i++)
// 		printf("%d ", dataSectors[i]);
// 	printf("\nFile contents:\n");


// 	if(numBytes > BYTES_4LEVEL){		// Level 1
// 		for(int i=0; i< numSectors/NumDirect; i++){	// If FULL: run 30 recursive calls
// 			OpenFile* opfile = new OpenFile(dataSectors[i]);
// 			FileHeader* subhdr = opfile->getHdr();
// 			subhdr->Print();
// 		}
// 	}
// 	else if(numBytes > BYTES_3LEVEL){ // Level 2
// 		for(int i=0; i< numSectors/NumDirect; i++){
// 			OpenFile* opfile = new OpenFile(dataSectors[i]);
// 			FileHeader* subhdr = opfile->getHdr();
// 			subhdr->Print();
// 		}
// 	}
// 	else if(numBytes > BYTES_2LEVEL){ // Level 3
// 		for(int i=0; i< numSectors/NumDirect; i++){
// 			OpenFile* opfile = new OpenFile(dataSectors[i]);
// 			FileHeader* subhdr = opfile->getHdr();
// 			subhdr->Print();
// 		}
// 	}
// 	else if(numBytes > BYTES_1LEVEL){ // Level 4
// 		for(int i=0; i< numSectors/NumDirect; i++){
// 			OpenFile* opfile = new OpenFile(dataSectors[i]);
// 			FileHeader* subhdr = opfile->getHdr();
// 			subhdr->Print();
// 		}
// 	}
// 	else{	// Level 5: True DataBlock
// 		for (i = k = 0; i < numSectors; i++)
// 		{
// 			kernel->synchDisk->ReadSector(dataSectors[i], data);
// 			for (j = 0; (j < SectorSize) && (k < numBytes); j++, k++)
// 			{
// 				if ('\040' <= data[j] && data[j] <= '\176') // isprint(data[j])
// 					printf("%c", data[j]);
// 				else
// 					printf("\\%x", (unsigned char)data[j]);
// 			}
// 			printf("\n");
// 		}
// 	}
// 	delete[] data;
	
// }
void FileHeader::PrintDataBlocks()
{
    int i, j, k;
    char *data = new char[SectorSize];

    for (i = k = 0; i < numSectors; i++)
    {
        kernel->synchDisk->ReadSector(dataSectors[i], data);
        for (j = 0; (j < SectorSize) && (k < numBytes); j++, k++)
        {
            if ('\040' <= data[j] && data[j] <= '\176') // isprint(data[j])
                printf("%c", data[j]);
            else
                printf("\\%x", (unsigned char)data[j]);
        }
        printf("\n");
    }

    delete[] data;
}

void FileHeader::PrintRecursive(int levels)
{
    if (levels > 0)
    {
        int numSubSectors = numSectors / NumDirect;
        for (int i = 0; i < numSubSectors; i++)
        {
            OpenFile *opfile = new OpenFile(dataSectors[i]);
            FileHeader *subhdr = opfile->getHdr();
            subhdr->PrintRecursive(levels - 1);
        }
    }
    else
    {
        PrintDataBlocks();
    }
}

void FileHeader::Print()
{
    printf("FileHeader contents.  File size: %d.  File blocks:\n", numBytes);
    for (int i = 0; i < numSectors; i++)
        printf("%d ", dataSectors[i]);
    printf("\nFile contents:\n");

    PrintRecursive(4);
}
