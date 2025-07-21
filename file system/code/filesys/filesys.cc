// filesys.cc
//	Routines to manage the overall operation of the file system.
//	Implements routines to map from textual file names to files.
//
//	Each file in the file system has:
//	   A file header, stored in a sector on disk
//		(the size of the file header data structure is arranged
//		to be precisely the size of 1 disk sector)
//	   A number of data blocks
//	   An entry in the file system directory
//
// 	The file system consists of several data structures:
//	   A bitmap of free disk sectors (cf. bitmap.h)
//	   A directory of file names and file headers
//
//      Both the bitmap and the directory are represented as normal
//	files.  Their file headers are located in specific sectors
//	(sector 0 and sector 1), so that the file system can find them
//	on bootup.
//
//	The file system assumes that the bitmap and directory files are
//	kept "open" continuously while Nachos is running.
//
//	For those operations (such as Create, Remove) that modify the
//	directory and/or bitmap, if the operation succeeds, the changes
//	are written immediately back to disk (the two files are kept
//	open during all this time).  If the operation fails, and we have
//	modified part of the directory and/or bitmap, we simply discard
//	the changed version, without writing it back to disk.
//
// 	Our implementation at this point has the following restrictions:
//
//	   there is no synchronization for concurrent accesses
//	   files have a fixed size, set when the file is created
//	   files cannot be bigger than about 3KB in size
//	   there is no hierarchical directory structure, and only a limited
//	     number of files can be added to the system
//	   there is no attempt to make the system robust to failures
//	    (if Nachos exits in the middle of an operation that modifies
//	    the file system, it may corrupt the disk)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.
#ifndef FILESYS_STUB
// #define DEBUG_OPENFILE
// #define DEBUG_TOKEN
#include "copyright.h"
#include "debug.h"
#include "disk.h"
#include "pbitmap.h"
#include "directory.h"
#include "filehdr.h"
#include "filesys.h"

// Sectors containing the file headers for the bitmap of free sectors,
// and the directory of files.  These file headers are placed in well-known
// sectors, so that they can be located on boot-up.
#define FreeMapSector 0
#define DirectorySector 1

// Initial file sizes for the bitmap and directory; until the file system
// supports extensible files, the directory size sets the maximum number
// of files that can be loaded onto the disk.
#define FreeMapFileSize (NumSectors / BitsInByte)
// #define NumDirEntries 10
#define DirectoryFileSize (sizeof(DirectoryEntry) * NumDirEntries)
// #define DEBUG_TOKEN

//----------------------------------------------------------------------
// FileSystem::FileSystem
// 	Initialize the file system.  If format = TRUE, the disk has
//	nothing on it, and we need to initialize the disk to contain
//	an empty directory, and a bitmap of free sectors (with almost but
//	not all of the sectors marked as free).
//
//	If format = FALSE, we just have to open the files
//	representing the bitmap and the directory.
//
//	"format" -- should we initialize the disk?
//----------------------------------------------------------------------

FileSystem::FileSystem(bool format)
{
    DEBUG(dbgFile, "Initializing the file system.");
    if (format)
    {
        PersistentBitmap *freeMap = new PersistentBitmap(NumSectors);
        Directory *directory = new Directory(NumDirEntries);
        FileHeader *mapHdr = new FileHeader;
        FileHeader *dirHdr = new FileHeader;

        DEBUG(dbgFile, "Formatting the file system.");

        // First, allocate space for FileHeaders for the directory and bitmap
        // (make sure no one else grabs these!)
        freeMap->Mark(FreeMapSector);
        freeMap->Mark(DirectorySector);

        // Second, allocate space for the data blocks containing the contents
        // of the directory and bitmap files.  There better be enough space!

        ASSERT(mapHdr->Allocate(freeMap, FreeMapFileSize));
        ASSERT(dirHdr->Allocate(freeMap, DirectoryFileSize));

        // Flush the bitmap and directory FileHeaders back to disk
        // We need to do this before we can "Open" the file, since open
        // reads the file header off of disk (and currently the disk has garbage
        // on it!).

        DEBUG(dbgFile, "Writing headers back to disk.");
        mapHdr->WriteBack(FreeMapSector);
        dirHdr->WriteBack(DirectorySector);

        // OK to open the bitmap and directory files now
        // The file system operations assume these two files are left open
        // while Nachos is running.

        freeMapFile = new OpenFile(FreeMapSector);
        directoryFile = new OpenFile(DirectorySector);

        // Once we have the files "open", we can write the initial version
        // of each file back to disk.  The directory at this point is completely
        // empty; but the bitmap has been changed to reflect the fact that
        // sectors on the disk have been allocated for the file headers and
        // to hold the file data for the directory and bitmap.

        DEBUG(dbgFile, "Writing bitmap and directory back to disk.");
        freeMap->WriteBack(freeMapFile); // flush changes to disk
        directory->WriteBack(directoryFile);

        if (debug->IsEnabled('f'))
        {
            freeMap->Print();
            directory->Print();
        }
        delete freeMap;
        delete directory;
        delete mapHdr;
        delete dirHdr;
    }
    else
    {
        // if we are not formatting the disk, just open the files representing
        // the bitmap and directory; these are left open while Nachos is running
        freeMapFile = new OpenFile(FreeMapSector);
        directoryFile = new OpenFile(DirectorySector);
    }
}

//----------------------------------------------------------------------
// MP4 mod tag
// FileSystem::~FileSystem
//----------------------------------------------------------------------
FileSystem::~FileSystem()
{
    delete freeMapFile;
    delete directoryFile;
}

//----------------------------------------------------------------------
// FileSystem::Create
// 	Create a file in the Nachos file system (similar to UNIX create).
//	Since we can't increase the size of files dynamically, we have
//	to give Create the initial size of the file.
//
//	The steps to create a file are:
//	  Make sure the file doesn't already exist
//        Allocate a sector for the file header
// 	  Allocate space on disk for the data blocks for the file
//	  Add the name to the directory
//	  Store the new file header on disk
//	  Flush the changes to the bitmap and the directory back to disk
//
//	Return TRUE if everything goes ok, otherwise, return FALSE.
//
// 	Create fails if:
//   		file is already in directory
//	 	no free space for file header
//	 	no free entry for file in directory
//	 	no free space for data blocks for the file
//
// 	Note that this implementation assumes there is no concurrent access
//	to the file system!
//
//	"name" -- name of file to be created
//	"initialSize" -- size of file to be created
//----------------------------------------------------------------------

// bool FileSystem::Create(char *name, int initialSize)
// {
//     Directory *directory;
//     PersistentBitmap *freeMap;
//     FileHeader *hdr;
//     int sector;
//     bool success;

//     DEBUG(dbgFile, "Creating file " << name << " size " << initialSize);

//     directory = new Directory(NumDirEntries);
//     directory->FetchFrom(directoryFile);

//     if (directory->Find(name) != -1)
//         success = FALSE; // file is already in directory
//     else
//     {
//         freeMap = new PersistentBitmap(freeMapFile, NumSectors);
//         sector = freeMap->FindAndSet(); // find a sector to hold the file header
//         if (sector == -1)
//             success = FALSE; // no free block for file header
//         else if (!directory->Add(name, sector))
//             success = FALSE; // no space in directory
//         else
//         {
//             hdr = new FileHeader;
//             if (!hdr->Allocate(freeMap, initialSize))
//                 success = FALSE; // no space on disk for data
//             else
//             {
//                 success = TRUE;
//                 // everthing worked, flush all changes back to disk
//                 hdr->WriteBack(sector);
//                 directory->WriteBack(directoryFile);
//                 freeMap->WriteBack(freeMapFile);
//             }
//             delete hdr;
//         }
//         delete freeMap;
//     }
//     delete directory;
//     return success;
// }
/*li3_1*/
bool FileSystem::Create(char *filePath, int initialSize)
{
    // cout<<"-------create------- "<<endl;
    // cout<<"my filePath: "<<filePath<<"\n";
    PersistentBitmap *freeMap;
    FileHeader *fileHeader;
    int sector;
    bool success;

    Directory *currentDir = new Directory(NumDirEntries);
    currentDir->FetchFrom(directoryFile);

    OpenFile *currentDirFile = directoryFile;
    OpenFile *lastLevelDirFile = currentDirFile;

    bool shouldRollBackToLastLevelDir = true;
    char *token = strtok(filePath, "/");
    char *prevToken = token;

    while (token != NULL) {
        sector = currentDir->Find(token);

        if (sector == -1) {
            shouldRollBackToLastLevelDir = false;
            break;
        }

        lastLevelDirFile = currentDirFile;
        currentDirFile = new OpenFile(sector);
        currentDir->FetchFrom(currentDirFile);

        prevToken = token;
        token = strtok(NULL, "/");
    }

    if (token == NULL) {
        token = prevToken;
    }

    if (shouldRollBackToLastLevelDir) {
        currentDir->FetchFrom(lastLevelDirFile);
    }

    // 檢查檔案是否已存在於目錄中
    if (currentDir->Find(token) != -1) {
        success = false;
    } else {
        // 分配空閒的磁區
        freeMap = new PersistentBitmap(freeMapFile, NumSectors);
        sector = freeMap->FindAndSet();

        if (sector == -1 || !currentDir->Add(token, sector, false)) {
            // 若分配磁區失敗或添加目錄項失敗，返回失敗
            success = false;
        } else {
            // 配置檔案頭
            fileHeader = new FileHeader;
            if (!fileHeader->Allocate(freeMap, initialSize)) {
                // 若分配檔案頭失敗，返回失敗
                success = false;
            } else {
                // 寫回檔案頭，目錄，和空閒位圖
                fileHeader->WriteBack(sector);
                currentDir->WriteBack(currentDirFile);
                freeMap->WriteBack(freeMapFile);
                success = true;
            }
            delete fileHeader;
        }
        delete freeMap;
    }
    delete currentDir;

    return success;
}



bool FileSystem::CreateDir(char *name) {
    PersistentBitmap *freeMap;
    FileHeader *fileHeader;
    int sector;
    bool success;

    Directory *currentDir = new Directory(NumDirEntries);
    currentDir->FetchFrom(directoryFile);

    OpenFile *currentDirFile = directoryFile;
    OpenFile *lastLevelDirFile = currentDirFile;

    bool shouldRollBackToLastLevelDir = true;
    char *token = strtok(name, "/");
    char *prevToken = token;

    while (token != NULL) {
        sector = currentDir->Find(token);

        if (sector == -1) {
            shouldRollBackToLastLevelDir = false;
            break;
        }

        lastLevelDirFile = currentDirFile;
        currentDirFile = new OpenFile(sector);
        currentDir->FetchFrom(currentDirFile);

        prevToken = token;
        token = strtok(NULL, "/");
    }

    if (token == NULL) {
        token = prevToken;
    }

    if (shouldRollBackToLastLevelDir) {
        currentDir->FetchFrom(lastLevelDirFile);
    }

    // 檢查目錄是否已存在於目錄中
    if (currentDir->Find(token) != -1) {
        success = false;
    } else {
        // 分配空閒的磁區
        freeMap = new PersistentBitmap(freeMapFile, NumSectors);
        sector = freeMap->FindAndSet();

        if (sector == -1 || !currentDir->Add(token, sector, true)) {
            // 若分配磁區失敗或添加目錄項失敗，返回失敗
            success = false;
        } else {
            // 配置檔案頭（此處可以根據需要進行相應的修改）
            fileHeader = new FileHeader;
            if (!fileHeader->Allocate(freeMap, DirectoryFileSize)) {
                // 若分配檔案頭失敗，返回失敗
                success = false;
            } else {
                // 寫回檔案頭，目錄，和空閒位圖
                fileHeader->WriteBack(sector);
                currentDir->WriteBack(currentDirFile);
                freeMap->WriteBack(freeMapFile);
                success = true;
            }
            delete fileHeader;
        }
        delete freeMap;
    }
    delete currentDir;

    return success;
}

//----------------------------------------------------------------------
// FileSystem::Open
// 	Open a file for reading and writing.
//	To open a file:
//	  Find the location of the file's header, using the directory
//	  Bring the header into memory
//
//	"name" -- the text name of the file to be opened
//----------------------------------------------------------------------

// OpenFile * FileSystem::Open(char *name)
// {
//     Directory *directory = new Directory(NumDirEntries);
//     OpenFile *openFile = NULL;
//     int sector;

//     DEBUG(dbgFile, "Opening file" << name);
//     directory->FetchFrom(directoryFile);
//     sector = directory->Find(name);
//     if (sector >= 0)
//         openFile = new OpenFile(sector); // name was found in directory
//     delete directory;
//     return openFile; // return NULL if not found
// }
/*li3_2*/
OpenFile * FileSystem::OpenAFile(char *name)
{
    //cout<<"----------------"<<endl;
    Directory *directory = new Directory(NumDirEntries);
    directory->FetchFrom(directoryFile);
    
    OpenFile *openFile = NULL;
    int sector;
    OpenFile *currentDirFile = directoryFile;
    
    // std::cout<<name<<std::endl;
    //cout<<"my name: "<<name<<"\n";
    char *token = strtok(name, "/");
    char *prevToken = token;    

    while (token != NULL)
    {
        sector = directory->Find(token);
        if (sector == -1 || !directory->isadirectory(token))
        {
            // 如果 sector 為 -1 或者不是目錄，退出循環
            break;
        }
        currentDirFile = new OpenFile(sector);
        directory->FetchFrom(currentDirFile);
        prevToken = token;
        token = strtok(NULL, "/");
        // 可以加入適當的邏輯處理當前 token
    }
    if(token==NULL){
        //cout<<"token == NULL\n";
        name = prevToken;
    }
    else{
        //cout<<"token != NULL\n";
        name = token; 

    }
    // cout<<"fuck__________________\n";
    // cout<< directory<<endl;
    sector = directory->Find(name); 
    if (sector >= 0){
        openFile = new OpenFile(sector); 
    }
    delete directory;
    opfile = openFile;
    return openFile; // return NULL if not found
}
/*li3_3*/
int FileSystem::WriteFile(char *BF, int size, OpenFileId id){
    return opfile->Write(BF, size);
}

int FileSystem::ReadFile(char *BF, int size, OpenFileId id){
    return opfile->Read(BF,size);
}

int FileSystem::CloseAFile(OpenFileId id){
    delete opfile;
    return 1;
}

//----------------------------------------------------------------------
// FileSystem::Remove
// 	Delete a file from the file system.  This requires:
//	    Remove it from the directory
//	    Delete the space for its header
//	    Delete the space for its data blocks
//	    Write changes to directory, bitmap back to disk
//
//	Return TRUE if the file was deleted, FALSE if the file wasn't
//	in the file system.
//
//	"name" -- the text name of the file to be removed
//----------------------------------------------------------------------
/*li3_4*/
bool FileSystem::Remove(char *name, bool Recursive)
{
    Directory *directory;
    PersistentBitmap *freeMap;
    FileHeader *fileHdr;
    int sector, last_sector;

    directory = new Directory(NumDirEntries);
    directory->FetchFrom(directoryFile);
    OpenFile *file_temp = directoryFile;
    OpenFile *last_level_dir_file = file_temp;
    bool should_roll_back_to_last_level_dir = true;

    char *token = strtok(name, "/");
    char *prev_token = token;

    for (; token != NULL; token = strtok(NULL, "/")) {
        last_sector = sector;
        sector = directory->Find(token);

        if (sector == -1) {
            std::cout << "No such file \'" << token << "\' in current directory. Break\n";
            delete directory;
            return false;
        }

        last_level_dir_file = file_temp;
        file_temp = new OpenFile(sector);
        directory->FetchFrom(file_temp);

        prev_token = token;
    }
    if(token==NULL){
        token = prev_token;
        directory->FetchFrom(last_level_dir_file);     //////// KEY!!!!
    }
    OpenFile *actual_dir_opfile = file_temp;
    fileHdr = new FileHeader;
    fileHdr->FetchFrom(sector);

    // KEY!!!!
    freeMap = new PersistentBitmap(freeMapFile, NumSectors);

    // Determine whether to perform recursive removal based on the Recursive flag
    if (Recursive) {
        // cout<<"Recursive remove\n";
        // // Check if the target is a directory
        // cout<<"prev_token: "<<prev_token<<endl;
        // cout<<"token: "<<token<<endl;
        
        if (directory->isadirectory(token)) {
            // cout<<"is a directory\n";
            directory->FetchFrom(actual_dir_opfile);
            directory->List();
            
            // Perform recursive removal
            // cout<<"token"<<token<<endl;
            // cout<<"recursive remove:"<<token<<"\n";
            directory->RecursiveRemove(token);
            
            // Flush changes to disk
            directory->WriteBack(actual_dir_opfile); // flush to disk   //////// KEY!!!! 
            directory->List();

            // Deallocate file header and clear free map for the target directory
            fileHdr->Deallocate(freeMap);
            freeMap->Clear(sector);

            // Roll back to the last level directory
            directory->FetchFrom(last_level_dir_file);
            // Remove the target directory entry from the last level directory
            directory->Remove(token);
            // Flush changes to disk
            directory->WriteBack(last_level_dir_file); // flush to disk   //////// KEY!!!!    
        } else {
            // If the target is not a directory, simply remove the file entry
            fileHdr->Deallocate(freeMap);
            freeMap->Clear(sector);
            directory->Remove(token);
            directory->WriteBack(last_level_dir_file); // flush to disk   //////// KEY!!!!
        }
    }
    else {
        // Check if the target is a directory
        if (directory->isadirectory(token)) {
            // If the target is a directory, return FALSE as Recursive removal is required
            return FALSE;
        }

        // Deallocate file header and clear free map for the target file
        fileHdr->Deallocate(freeMap);
        freeMap->Clear(sector);

        // Remove the target file entry
        directory->Remove(token);

        // Flush changes to disk
        directory->WriteBack(last_level_dir_file); // flush to disk   //////// KEY!!!!
    }
    freeMap->WriteBack(freeMapFile);
    delete fileHdr;
    delete directory;
    delete freeMap;
    return TRUE;
}
bool FileSystem::Remove(char *name)
{
    Directory *directory;
    PersistentBitmap *freeMap;
    FileHeader *fileHdr;
    int sector;

    directory = new Directory(NumDirEntries);
    directory->FetchFrom(directoryFile);
    sector = directory->Find(name);
    if (sector == -1)
    {
        delete directory;
        return FALSE; // file not found
    }
    fileHdr = new FileHeader;
    fileHdr->FetchFrom(sector);

    freeMap = new PersistentBitmap(freeMapFile, NumSectors);

    fileHdr->Deallocate(freeMap); // remove data blocks
    freeMap->Clear(sector);       // remove header block
    directory->Remove(name);

    freeMap->WriteBack(freeMapFile);     // flush to disk
    directory->WriteBack(directoryFile); // flush to disk
    delete fileHdr;
    delete directory;
    delete freeMap;
    return TRUE;
}

//----------------------------------------------------------------------
// FileSystem::List
// 	List all the files in the file system directory.
//----------------------------------------------------------------------
/*li3_5*/
void FileSystem::List(char *dirname, bool Recursive)
{
    Directory *directory = new Directory(NumDirEntries);
    directory->FetchFrom(directoryFile);

    OpenFile *dirFile = directoryFile;
    int sector;

    char *token = strtok(dirname, "/");
    char *prev_token = dirname;

    for (; token != NULL; token = strtok(NULL, "/")) {
        if ((sector = directory->Find(token)) == -1) {
            printf("No such file or directory: %s\n", token);
            delete directory;
            return;
        }

        dirFile = new OpenFile(sector);
        directory->FetchFrom(dirFile);
        prev_token = token;
    }

    if (strcmp(prev_token, "/") == 0) {
        sector = DirectorySector;
    }

    if (Recursive) {
        directory->RecursiveList(0);
    } else {
        directory->List();
    }

    delete directory;
}


void FileSystem::List()
{
    Directory *directory = new Directory(NumDirEntries);

    directory->FetchFrom(directoryFile);
    directory->List();
    delete directory;
}

//----------------------------------------------------------------------
// FileSystem::Print
// 	Print everything about the file system:
//	  the contents of the bitmap
//	  the contents of the directory
//	  for each file in the directory,
//	      the contents of the file header
//	      the data in the file
//----------------------------------------------------------------------

void FileSystem::Print()
{
    FileHeader *bitHdr = new FileHeader;
    FileHeader *dirHdr = new FileHeader;
    PersistentBitmap *freeMap = new PersistentBitmap(freeMapFile, NumSectors);
    Directory *directory = new Directory(NumDirEntries);

    printf("Bit map file header:\n");
    bitHdr->FetchFrom(FreeMapSector);
    bitHdr->Print();

    printf("Directory file header:\n");
    dirHdr->FetchFrom(DirectorySector);
    dirHdr->Print();

    freeMap->Print();

    directory->FetchFrom(directoryFile);
    directory->Print();

    delete bitHdr;
    delete dirHdr;
    delete freeMap;
    delete directory;
}

#endif // FILESYS_STUB
