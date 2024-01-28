// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include <xtl.h>
#include <xboxmath.h>
#include <xbdm.h>

#pragma region Required Types & Defines
typedef long	NTSTATUS;
typedef struct _IO_STATUS_BLOCK {
	union {
		NTSTATUS Status; // 0x0 sz:0x4
		void * Pointer; // 0x0 sz:0x4
	} st;
	ULONG_PTR Information; // 0x4 sz:0x4
} IO_STATUS_BLOCK, *PIO_STATUS_BLOCK; // size 8
C_ASSERT(sizeof(IO_STATUS_BLOCK) == 0x8);

typedef struct _STRING {
	USHORT Length;
	USHORT MaximumLength;
	PCHAR Buffer;
} STRING, *PSTRING;

typedef PSTRING	POBJECT_STRING;

#define MAKE_STRING(s)   {(USHORT)(strlen(s)), (USHORT)((strlen(s))+1), (PCHAR)s} // use in code

typedef struct _OBJECT_ATTRIBUTES {
	HANDLE RootDirectory;
	POBJECT_STRING ObjectName;
	ULONG Attributes;
} OBJECT_ATTRIBUTES, *POBJECT_ATTRIBUTES;


#define FILE_SYNCHRONOUS_IO_NONALERT	0x00000020

#define NOP 0x60000000

#pragma endregion

// TODO: reference additional headers your program requires here
