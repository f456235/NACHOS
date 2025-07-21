// directory.cc
//	Routines to manage a directory of file names.
//
//	The directory is a table of fixed length entries; each
//	entry represents a single file, and contains the file name,
//	and the location of the file header on disk.  The fixed size
//	of each directory entry means that we have the restriction
//	of a fixed maximum size for file names.
//
//	The constructor initializes an empty directory of a certain size;
//	we use ReadFrom/WriteBack to fetch the contents of the directory
//	from disk, and to write back any modifications back to disk.
//
//	Also, this implementation has the restriction that the size
//	of the directory cannot expand.  In other words, once all the
//	entries in the directory are used, no more files can be created.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "utility.h"
#include "filehdr.h"
#include "directory.h"
#include "filesys.h"
#include "kernel.h"
#include "main.h"

//----------------------------------------------------------------------
// Directory::Directory
// 	Initialize a directory; initially, the directory is completely
//	empty.  If the disk is being formatted, an empty directory
//	is all we need, but otherwise, we need to call FetchFrom in order
//	to initialize it from disk.
//
//	"size" is the number of entries in the directory
//----------------------------------------------------------------------

Directory::Directory(int size)
{
    table = new DirectoryEntry[size];

    // MP4 mod tag
    memset(table, 0, sizeof(DirectoryEntry) * size); // dummy operation to keep valgrind happy

    tableSize = size;
    for (int i = 0; i < tableSize; i++){
        table[i].inUse = FALSE;
        /*li3*/
        table[i].isadirectory = FALSE;
    }
}

//----------------------------------------------------------------------
// Directory::~Directory
// 	De-allocate directory data structure.
//----------------------------------------------------------------------

Directory::~Directory()
{
    delete[] table;
}

//----------------------------------------------------------------------
// Directory::FetchFrom
// 	Read the contents of the directory from disk.
//
//	"file" -- file containing the directory contents
//----------------------------------------------------------------------

void Directory::FetchFrom(OpenFile *file)
{
    (void)file->ReadAt((char *)table, tableSize * sizeof(DirectoryEntry), 0);
}

//----------------------------------------------------------------------
// Directory::WriteBack
// 	Write any modifications to the directory back to disk
//
//	"file" -- file to contain the new directory contents
//----------------------------------------------------------------------

void Directory::WriteBack(OpenFile *file)
{
    (void)file->WriteAt((char *)table, tableSize * sizeof(DirectoryEntry), 0);
}

//----------------------------------------------------------------------
// Directory::FindIndex
// 	Look up file name in directory, and return its location in the table of
//	directory entries.  Return -1 if the name isn't in the directory.
//
//	"name" -- the file name to look up
//----------------------------------------------------------------------

int Directory::FindIndex(char *name)
{
    // cout<<"name: "<<name<<endl;
    // cout<<"tablesize: "<<tableSize<<endl;
    // cout<<"table: "<<table<<endl;
    for (int i = 0; i < tableSize; i++){
        // if(i<5) cout << "table["<<i<<"].name: " << table[i].name << endl;
        if (table[i].inUse && !strncmp(table[i].name, name, FileNameMaxLen))
        {
            return i;
        }
    }     
    return -1; // name not in directory
}

//----------------------------------------------------------------------
// Directory::Find
// 	Look up file name in directory, and return the disk sector number
//	where the file's header is stored. Return -1 if the name isn't
//	in the directory.
//
//	"name" -- the file name to look up
//----------------------------------------------------------------------

int Directory::Find(char *name)
{
    int i = FindIndex(name);

    if (i != -1)
        return table[i].sector;
    return -1;
}

//----------------------------------------------------------------------
// Directory::Add
// 	Add a file into the directory.  Return TRUE if successful;
//	return FALSE if the file name is already in the directory, or if
//	the directory is completely full, and has no more space for
//	additional file names.
//
//	"name" -- the name of the file being added
//	"newSector" -- the disk sector containing the added file's header
//----------------------------------------------------------------------
/*li3_8*/
bool Directory::Add(char *name, int newSector, bool isadirectory)
{
    if (FindIndex(name) != -1)
        return FALSE;

    // Find free entry in the directory table, set inUse to TRUE and string copy!

    for (int i = 0; i < tableSize; i++)
        if (!table[i].inUse)
        {
            table[i].inUse = TRUE;
            strncpy(table[i].name, name, FileNameMaxLen); // string copy: copy 'name' to table[i].name, no more than 'FileNameMaxLen' characters
            table[i].sector = newSector;
            table[i].isadirectory = isadirectory; /*change*/
            return TRUE;
        }
    return FALSE; // no space.  Fix when we have extensible files.
}
bool Directory::Add(char *name, int newSector)
{
    if (FindIndex(name) != -1)
        return FALSE;

    for (int i = 0; i < tableSize; i++)
        if (!table[i].inUse)
        {
            table[i].inUse = TRUE;
            strncpy(table[i].name, name, FileNameMaxLen);
            table[i].sector = newSector;
            return TRUE;
        }
    return FALSE; // no space.  Fix when we have extensible files.
}

//----------------------------------------------------------------------
// Directory::Remove
// 	Remove a file name from the directory.  Return TRUE if successful;
//	return FALSE if the file isn't in the directory.
//
//	"name" -- the file name to be removed
//----------------------------------------------------------------------

bool Directory::Remove(char *name)
{
    int i = FindIndex(name);

    if (i == -1)
        return FALSE; // name not in directory
    table[i].inUse = FALSE;
    return TRUE;
}
/*li3_11*/
void Directory::RecursiveRemove(char *name)
{
    int index = 0;
    while (index < tableSize) {
        if (table[index].inUse) {
            if (table[index].isadirectory) {
                // cout << "Remove directory: " << table[index].name << endl;
                Directory *nextDir = new Directory(NumDirEntries);
                OpenFile *nextDirFile = new OpenFile(table[index].sector);    // (FCB of the next_directory file. ) =>Open from FCB.
                nextDir->FetchFrom(nextDirFile);

                PersistentBitmap *freeMap = new PersistentBitmap(kernel->fileSystem->getFreeMapFile(), NumSectors);
                FileHeader *nextDirFileToRemove = new FileHeader;
                nextDirFileToRemove->FetchFrom(table[index].sector);

                nextDirFileToRemove->Deallocate(freeMap); // remove data blocks
                freeMap->Clear(table[index].sector);

                nextDir->RecursiveRemove(table[index].name);

                nextDir->WriteBack(nextDirFile);

                this->Remove(table[index].name); /* KEY!!!! */
                freeMap->WriteBack(kernel->fileSystem->getFreeMapFile());

                delete nextDirFile;
                delete nextDir;
                delete nextDirFileToRemove;
                delete freeMap;
            } else {
                // cout << "Remove file: " << table[index].name << endl;
                PersistentBitmap *freeMap = new PersistentBitmap(kernel->fileSystem->getFreeMapFile(), NumSectors);
                FileHeader *fileHdrOfFileToRemove = new FileHeader;
                fileHdrOfFileToRemove->FetchFrom(table[index].sector);
                fileHdrOfFileToRemove->Deallocate(freeMap); 
                freeMap->Clear(table[index].sector); 
                freeMap->WriteBack(kernel->fileSystem->getFreeMapFile());

                delete fileHdrOfFileToRemove;
                delete freeMap;
            }
        }
        index++;
    }    
}

//----------------------------------------------------------------------
// Directory::List
// 	List all the file names in the directory.
//----------------------------------------------------------------------

// void Directory::List()
// {
//     for (int i = 0; i < tableSize; i++)
//         if (table[i].inUse)
//             printf("%s\n", table[i].name);
// }
/*li3_9*/
void Directory::List()
{
    //cout<<"print\n";
    bool empty = true;
    int i = 0;
    while (i < tableSize) {
        // std::cout<<"Entry "<<i<<"inUse: "<<table[i].inUse<<", name: "<<table[i].name<<", sector: "<<table[i].sector<<"\n";
        if (table[i].inUse == true) {
            empty = false;
            // printf("%s, %d\n",table[i].name, table[i].isadirectory);
            if (table[i].isadirectory == true)
                //     std::cout<<"[D] "<<table[i].name<<std::endl;
                // else std::cout<<"[F] "<<table[i].name<<std::endl;
                std::cout << table[i].name << std::endl;
            else
                std::cout << table[i].name << std::endl;
        }
        // else std::cout<<"Entry "<<i<<" not used.\n";
        ++i;
    }
    if (empty)
        std::cout << "The directory is empty\n";
}
/*li3_10*/
void Directory::RecursiveList(int depth)
{
    // bool empty = true;
    Directory *subdir = new Directory(NumDirEntries);
    OpenFile *subdirOpenFile;

    int i = 0;
    // for(i=0;i<64;i++)
    // {
    //     // if (table[i].isadirectory)
    //     // {
    //     //     printf("[D] %s", table[i].name);
    //     //     std::cout << "\n";
    //     //     subdirOpenFile = new OpenFile(table[i].sector);
    //     //     subdir->FetchFrom(subdirOpenFile);
    //     //     subdir->RecursiveList(depth + 1);
    //     // }
    //     // else
    //     // {
    //     //     printf("[F] %s", table[i].name);
    //     //     std::cout << "\n";
    //     // }
    //     cout<<"table["<<i<<"].inUse: "<<table[i].inUse<<"\n";
    // }
    while (i < tableSize )
    {
        if(table[i].inUse)
        {
            // empty = false;
            int k = 0;
            while (k < depth)
            {
                std::cout << "    ";
                k++;
            }

            if (table[i].isadirectory)
            {
                printf("[D] %s", table[i].name);
                std::cout << "\n";
                subdirOpenFile = new OpenFile(table[i].sector);
                subdir->FetchFrom(subdirOpenFile);
                subdir->RecursiveList(depth + 1);
            }
            else
            {
                printf("[F] %s", table[i].name);
                std::cout << "\n";
            }
        }
        // if(i==1) cout<<"table["<<i<<"].name"<<table[i].name<<"\n";
        // cout<<"i: "<<i<<"\n";
        i++;
    }
    delete subdir;
}
//----------------------------------------------------------------------
// Directory::Print
// 	List all the file names in the directory, their FileHeader locations,
//	and the contents of each file.  For debugging.
//----------------------------------------------------------------------

void Directory::Print()
{
    FileHeader *hdr = new FileHeader;

    printf("Directory contents:\n");
    for (int i = 0; i < tableSize; i++)
        if (table[i].inUse)
        {
            printf("Name: %s, Sector: %d\n", table[i].name, table[i].sector);
            hdr->FetchFrom(table[i].sector);
            hdr->Print();
        }
    printf("\n");
    delete hdr;
}
