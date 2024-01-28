//===============================================================================================================================================
//
//					XellSB_17489 - An application designed to shadowboot a running Devkit into Xell (Can be adapted for any Devkit shadowboot)
//
// Created by Byrom - https://github.com/Byrom90
//
// Credits:
//			- Visual Studio / GoobyCorp - https://github.com/GoobyCorp
//			- RGLoader - https://github.com/RGLoader
//			- Swizzy - https://github.com/Swizzy
//			- c0z
//			- Free60 Project / libxenon - https://github.com/Free60Project/
//			- Emma / InvoxiPlayGames - https://github.com/InvoxiPlayGames
//			- Josh / Octal450 - https://github.com/Octal450
//			- Members of the Xbox 360 Hub Discord server - Specifically the #coding-corner - https://xbox360hub.com/
//			- Anyone else I missed who has contributed to the wider 360 scene
//
//===============================================================================================================================================

#include "stdafx.h"
#include "ShadowbootFiles.h"

// A lot of this can be optimised / simplified
// This is just a rough setup I put together for testing

#pragma region Additional Requirements
typedef struct _XBOX_KRNL_VERSION {
	WORD Major;
	WORD Minor;
	WORD Build;
	WORD Qfe;
} XBOX_KRNL_VERSION, *PXBOX_KRNL_VERSION;

typedef struct _XBOX_HARDWARE_INFO {
	DWORD Flags;
	unsigned char NumberOfProcessors;
	unsigned char PCIBridgeRevisionID;
	unsigned char Reserved[6];
	unsigned short BldrMagic;
	unsigned short BldrFlags;
} XBOX_HARDWARE_INFO, *PXBOX_HARDWARE_INFO;

// xkelib could be used here to make things easier
EXTERN_C { 
	VOID DbgPrint(const char* s, ...);
	extern PXBOX_KRNL_VERSION XboxKrnlVersion;
	extern PXBOX_HARDWARE_INFO XboxHardwareInfo;
	DWORD MmGetPhysicalAddress(void *buffer);
	DWORD ExCreateThread(PHANDLE pHandle, DWORD dwStackSize, LPDWORD lpThreadId, PVOID apiThreadStartup, 
		LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter, DWORD dwCreationFlagsMod);
	VOID XapiThreadStartup(VOID (__cdecl *StartRoutine)(VOID *), PVOID StartContext, DWORD dwExitCode);
	NTSTATUS NtOpenFile(PHANDLE FileHandle, ACCESS_MASK DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes, 
		PIO_STATUS_BLOCK IoStatusBlock, DWORD ShareAccess, DWORD OpenOptions);
	NTSTATUS NtClose(HANDLE Handle);
}

VOID CreateSystemThread(void* ptr, void* p = NULL)
{
	HANDLE hThread1 = 0; DWORD threadId1 = 0;
	ExCreateThread(&hThread1, 0x10000, &threadId1, (VOID*)XapiThreadStartup, (LPTHREAD_START_ROUTINE)ptr, p, 0x2);
	XSetThreadProcessor(hThread1, 4);
	ResumeThread(hThread1);
	CloseHandle(hThread1);
}
#pragma endregion

#pragma region XAM
#define NAME_XAM "xam.xex"
#define XNotifyQueueUI_ORD 656
typedef VOID (*XNotifyQueueUI)(int type, DWORD dwUserIndex, ULONGLONG qwAreas, PWCHAR displayText, PVOID contextData);
XNotifyQueueUI XNotify = NULL;

VOID ShowNotify(PWCHAR NotifyText)
{
	if (!XNotify)
		XNotify = (XNotifyQueueUI)GetProcAddress(GetModuleHandle(NAME_XAM), (LPCSTR)XNotifyQueueUI_ORD);
	XNotify(14, 0, 2, NotifyText, NULL);
}
#pragma endregion

#pragma region XBDM
#define NAME_XBDM "xbdm.xex"
//#define XBDM_NOERR 0x02DA0000
#define DMCT_DEVELOPMENT_KIT	0

//#define DMGETCONSOLETYPE_ORD 140
//typedef HRESULT(*DMGETCONSOLETYPE)(DWORD* pdwConsoleType);
//DMGETCONSOLETYPE DmGetConsoleType = NULL;

// Check the console is a Devkit since we don't support Testkit
BOOL IsDevkitHardware()
{
	//if (!DmGetConsoleType)
	//	DmGetConsoleType = (DMGETCONSOLETYPE)GetProcAddress(GetModuleHandle(NAME_XBDM), (LPCSTR)DMGETCONSOLETYPE_ORD);
 
	DWORD XDKCONTYPE;
	HRESULT GetHWType = DmGetConsoleType(&XDKCONTYPE);
	if (GetHWType == XBDM_NOERR && XDKCONTYPE == DMCT_DEVELOPMENT_KIT) // Hardware is Devkit
		return TRUE;

	return FALSE; // Error or the current hardware is Testkit or Demokit
}

//#define DMSETMEMORY_ORD 40
//typedef HRESULT(*DMSETMEMORY)(LPVOID lpbAddr, DWORD cb, LPCVOID lpbBuf, LPDWORD pcbRet);
//DMSETMEMORY DmSetMemory = NULL;

// Set memory on consoles with memory protections in place
BOOL XBDMSetMemory(VOID* DestAddr, VOID* Source, DWORD Length)
{
	//if (!DmSetMemory)
	//	DmSetMemory = (DMSETMEMORY)GetProcAddress(GetModuleHandle(NAME_XBDM), (LPCSTR)DMSETMEMORY_ORD);
	
	DWORD NumBytesWritten; // Pass this to receive number of bytes written. Otherwise return value will be 0x82DA0004 XBDM_MEMUNMAPPED
	HRESULT Result = DmSetMemory(DestAddr, Length, Source, &NumBytesWritten);
	if (Result != XBDM_NOERR)
	{
		DbgPrint("Failed to set memory using XBDM! Result: %08X\n", Result);
		return FALSE;
	}

	return TRUE;
}

// Any additional patches can be added here. Not required currently
BYTE RequiredPatches[] = { 
	
	// End of patches
	0xFF, 0xFF, 0xFF, 0xFF
};

// Apply the patches above using XBDM
BOOL ApplyAdditionalPatches(BYTE* buffer, DWORD size)
{
	DWORD patchesApplied = 0, offset = 0;
	//DbgPrint("Patchset size: %i\n", size);
	DWORD dest = *(DWORD*)&buffer[offset];
	
	offset += 4;

	while (dest != 0xFFFFFFFF && offset < size) {
		//DbgPrint("dest: %08X\n", dest);
		DWORD numPatches = *(DWORD*)&buffer[offset];
		//DbgPrint("numPatches: %X\n", numPatches);
		offset += 4;
		for (int i = 0; i < numPatches; i++, offset += 4, dest += 4) {
			if (!XBDMSetMemory((PVOID)dest, &buffer[offset], 4))
				return FALSE;
			//DbgPrint("%08X -> 0x%08X\n", dest, *(DWORD*)&buffer[offset]);
		}
		dest = *(DWORD*)&buffer[offset];
		offset += 4;
		patchesApplied++;
	}
	DbgPrint("[ApplyAdditionalPatches] Complete! Num of patches applied: %i\n", patchesApplied);
	return TRUE;
}

#pragma endregion

#pragma region BOOTLOADER CHECK
#define TYPE_FAILED		0
#define REAL_DEVKIT		1
#define RGH_JTAG_DEV	2

#define CB_SB_OFFSET	0x8000 // Location of CB | CB_A | SB in flash
// Detemines whether it's a real Devkit or some sort of RGH/JTAG setup running dev kernel (XDKBuild for example)
// We use this to set whether to use the shadowboot with broken SB signature or not
// Additional patches are required to use the NOSIG version (XDKBuild has these)
int GetDevType()
{
	HANDLE hFile;
	OBJECT_ATTRIBUTES atFlash;
	IO_STATUS_BLOCK ioFlash;
	DWORD bRead = 0, dwPos;
	DWORD returnstatus;
	STRING nFlash = MAKE_STRING("\\Device\\Flash");
	atFlash.RootDirectory = 0;
	atFlash.ObjectName = &nFlash;
	atFlash.Attributes = FILE_ATTRIBUTE_DEVICE;
	returnstatus = NtOpenFile(&hFile, GENERIC_READ, &atFlash, &ioFlash, OPEN_EXISTING, FILE_SYNCHRONOUS_IO_NONALERT);
	if (returnstatus != 0)
	{
		DbgPrint("[GetDevType] NtOpenFile(flash): %08x (err: %08x)\n", returnstatus, GetLastError());
		return TYPE_FAILED;
	}
	BYTE MagicBuf[2];
	ZeroMemory(MagicBuf, 2);
	dwPos = SetFilePointer(hFile, CB_SB_OFFSET, NULL, FILE_BEGIN);
	if (dwPos != INVALID_SET_FILE_POINTER)
	{
		
		ReadFile(hFile, MagicBuf, 2, &bRead, NULL);
		NtClose(hFile);
	}
	else
	{
		DbgPrint("[GetDevType] SetFilePointer(flash) invalid! %08x\n", returnstatus, GetLastError());
		return TYPE_FAILED;
	}
		

	DbgPrint("[GetDevType] Complete!\n");
	
	// Result based on the magic of the bootloader it detects here CB / SB
	USHORT Magic = ((MagicBuf[0] & 0xFF) << 8) | (MagicBuf[1] & 0xFF);
	switch(Magic)
	{
	case 0x4342: // CB bootloader
		DbgPrint("[GetDevType] Modded console running Dev kernel\n");
		return RGH_JTAG_DEV;
	case 0x5342: // SB bootloader
		DbgPrint("[GetDevType] Real Devkit\n");
		return REAL_DEVKIT;
	default:
		DbgPrint("[GetDevType] Error\n");
		return TYPE_FAILED;
	}
}
#pragma endregion

#pragma region KERNEL
#define NAME_KERNEL "xboxkrnl.exe"
#define DbgUnLoadImageSymbols_ORD 423
typedef VOID DbgUnLoadImageSymbolsTypeDef(DWORD r3, DWORD r4, DWORD r5, DWORD r6);

// Not required. Testing only
DbgUnLoadImageSymbolsTypeDef* DbgUnLoadImageSymbols = NULL;
VOID TestUloadSymbols()
{
	// Test unloading debugsymbols
	// DbgUnLoadImageSymbols kernel export 423
	// registers before load
	// li        r6, 0
	// li        r5, 0
	// li        r4, -1
	// li        r3, 0
	// bl        DbgUnLoadImageSymbols
	DbgUnLoadImageSymbols = (DbgUnLoadImageSymbolsTypeDef*)GetProcAddress(GetModuleHandle(NAME_KERNEL), (LPCSTR)DbgUnLoadImageSymbols_ORD);
	DbgPrint("DbgUnLoadImageSymbolsAddr: %08X\n", DbgUnLoadImageSymbols);
	DbgUnLoadImageSymbols(0, -1, 0, 0);
}

// Could potentially figure a way to resolve some of these addresses automatically to support other kernel versions

// Have yet to determine why using this method of shadowbooting fails when KdNet is disabled so we check that before proceeding
// Checks KdNet is enabled. Pulls a single byte from the KdNetData populated during boot. 1 Enabled | 0 Disabled
#define KdNetIsEnabled_Addr_17489 0x801B97B8
typedef BOOL KdNetIsEnabledTypeDef();
KdNetIsEnabledTypeDef* KdNetIsEnabled = (KdNetIsEnabledTypeDef*)KdNetIsEnabled_Addr_17489;
BOOL GetKdNetEnableState()
{
	if (KdNetIsEnabled())
	{
		DbgPrint("[GetKdNetEnableState] KdNet is enabled!\n");
		return TRUE;
	}
	else
	{
		DbgPrint("[GetKdNetEnableState] KdNet is disabled!\n");
		return FALSE;
	}
}

#define KiShadowBoot_Addr_17489 0x800966A0
typedef void KiShadowBootTypeDef(DWORD dwAddr, DWORD dwSize, DWORD dwFlags);
KiShadowBootTypeDef* KiShadowBoot = (KiShadowBootTypeDef*)KiShadowBoot_Addr_17489;

// Can easily be adapted to shadowboot from a file you select in a graphical UI (or place next to this app xex)
VOID ShadowbootNowThread() // Makes no difference running from system thread or not ¯\_(ツ)_/¯ 
{
	int DevBLType = GetDevType();
	if (!DevBLType)
	{
		DbgPrint("[ShadowbootNow] Failed to determine console type! Exiting...\n");
		//ShowNotify(L"Failed to determine console type! Exiting..."); // Ideally want to avoid calling ui functions from system thread
		return;
	}
	DbgPrint("[ShadowbootNow] Attempting to shadowboot into xell...\n");

	// Currently both versions of the shadowboot are identical in size
	// If this changes be sure to change any sizeof sections to suit
	VOID* pPhysExp = XPhysicalAlloc(sizeof(xell_shadowboot), MAXULONG_PTR, 0, PAGE_READWRITE);
	DWORD physExpAdd = (DWORD)MmGetPhysicalAddress(pPhysExp);
	ZeroMemory(pPhysExp, sizeof(xell_shadowboot));
	memcpy(pPhysExp, DevBLType == REAL_DEVKIT ? xell_shadowboot : xell_shadowboot_NOSIG, sizeof(xell_shadowboot));
	// 0x200 or 0x600 is used depending on what r5 is set to in ExpTryToShadowBoot before calling ExpTryToBootMediaKernel and finally KiShadowboot
	// r5 is set to 1 for cdrom shadowbooting only which results in the flag being set to 0x600
	KiShadowBoot(physExpAdd, sizeof(xell_shadowboot), 0x200);
	// NOTE: No mem cleanup should be required since the system halts and reboots...
}
#pragma endregion

#pragma region Main App Function / Entry Point
//-------------------------------------------------------------------------------------
// Name: main()
// Desc: The application's entry point
//-------------------------------------------------------------------------------------
void __cdecl main()
{
	// Simple dev check to confirm it's not running on retail kernel
	if (*(DWORD*)0x8E038610 & 0x8000)
	{
		DbgPrint("Shadowbooting is not supported on Retail kernel! Exiting...\n");
		ShowNotify(L"Shadowbooting is not supported on Retail kernel! Exiting...");
		return;
	}
	// Needs to be a less than check so we still run on higher spoofed kernel versions
	// Very much doubt we will see any higher official kernel versions anyway
	if (XboxKrnlVersion->Build < 17489) 
	{
		DbgPrint("Unsupported kernel version detected! Exiting...\n");
		ShowNotify(L"Unsupported kernel version detected! Exiting...");
		return;
	}
	// Additional check to confirm it's running on Devkit rather than Testkit/Demo
	// There are likely better ways to check but this works ¯\_(ツ)_/¯ 
	if (!IsDevkitHardware())
	{
		DbgPrint("Unsupported hardware type detected! Exiting...\n");
		ShowNotify(L"Unsupported hardware type detected! Exiting...");
		return;
	}
	// Shadowbooting called using this method appears to hang/crash when KdNet is disabled
	if (!GetKdNetEnableState())
	{
		DbgPrint("KdNet must be enabled to shadowboot using this app! Exiting...\n");
		ShowNotify(L"KdNet must be enabled to shadowboot using this app! Exiting...");
		return;
	}

	// If everything checks out proceed to shadowboot 
	//CreateSystemThread(ShadowbootNowThread); // Using system thread doesn't seem to make any difference...
	ShadowbootNowThread();
	
	Sleep(5000); // Delay before exiting just in case...
}
#pragma endregion



// ExpTryToBootMediaKernel
/*
r3 - Pointer to file path string? - ffffffff80040510 when loaded for hdd option

r4 - Bool to determine whether it's a remote file or not. 1 - remote file | 0 hdd/flash/cdrom - 0 when loaded for hdd option

r5 - Bool to determine whether it's being loaded from cdrom or not. 1 - cdrom | 0 - hdd/flash - 0 when loaded for hdd option || Determines flag used by KiShadowBoot - 1 (0x600) | 0 (0x200)
*/
