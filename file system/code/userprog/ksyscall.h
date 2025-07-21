/**************************************************************
 *
 * userprog/ksyscall.h
 *
 * Kernel interface for systemcalls 
 *
 * by Marcus Voelp  (c) Universitaet Karlsruhe
 *
 **************************************************************/

#ifndef __USERPROG_KSYSCALL_H__
#define __USERPROG_KSYSCALL_H__

#include "kernel.h"

#include "synchconsole.h"

void SysHalt()
{
	kernel->interrupt->Halt();
}

int SysAdd(int op1, int op2)
{
	return op1 + op2;
}
/*li1_1*/
int SysCreate(char *name, int Size)
{
	return kernel->fileSystem->Create(name, Size);
}
OpenFileId SysOpen(char *name)
{
	OpenFileId id = (OpenFileId) kernel->fileSystem->OpenAFile(name);
	return id;
}
int SysWrite(char *buffer, int size, OpenFileId id)
{
        return kernel->fileSystem->WriteFile(buffer, size, id);
}

int SysRead(char *buffer, int size, OpenFileId id)
{
        return kernel->fileSystem->ReadFile(buffer, size, id);
}

int SysClose(OpenFileId id)
{
        return kernel->fileSystem->CloseAFile(id);
}
#ifdef FILESYS_STUB
int SysCreate(char *filename)
{
	// return value
	// 1: success
	// 0: failed
	return kernel->interrupt->CreateFile(filename);
}
#endif

#endif /* ! __USERPROG_KSYSCALL_H__ */
