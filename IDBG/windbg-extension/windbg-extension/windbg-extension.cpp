#include "stdafx.h"
#include "idbg.h"
#pragma comment(lib, "dbgeng.lib")


/*
You need the WDK to build this.


todo:
add an option on idbg that sets a bp automatically when you want to add a bp so you don't have to wait for a response from dbgeng



Credit: WDK samples 
*/


#define BUF_SIZE 16 


ULONG SavedMajorVersion;
ULONG SavedMinorVersion;
WINDBG_EXTENSION_APIS ExtensionApis = { 0 };
EXT_API_VERSION g_ExtApiVersion = { 1,1,EXT_API_VERSION_NUMBER,0 };
IDBG g_IDBG;


HANDLE hBpsFile = NULL;
HANDLE hMainThread = NULL;
bool ShouldUnload = false;
char BpsBufferName[] = "Local\\breakpoints_shared_memory";
char *BpsSharedBuffer = nullptr;


bool CreateSharedBuffers()
{
	hBpsFile = CreateFileMappingA(INVALID_HANDLE_VALUE,
		NULL,
		PAGE_READWRITE,
		0,
		BUF_SIZE,
		BpsBufferName);

	
	if (!hBpsFile)
	{
		dprintf("failed to create filemapping %p \n", GetLastError());
		return false;
	}


	BpsSharedBuffer = (char*)MapViewOfFile(hBpsFile, FILE_MAP_ALL_ACCESS, 0, 0, BUF_SIZE);

	if (!BpsSharedBuffer)
		return false;

	memset(BpsSharedBuffer, 0, BUF_SIZE);

	return true;
}

void RespondToBreakpointRequest(bool function_succeded)
{
	if (!function_succeded)
	{
		BpsSharedBuffer[0] = 'f';
		//dprintf("failed to add/remove breakpoint \n");
	}

	else
	{
		BpsSharedBuffer[0] = 'c';
		//dprintf("%p should be added/removed properly  \n", *(ULONG64*)(BpsSharedBuffer + 1));
	}

	//now we waint until IDBG gets the respond before we call HandleBreakpoints again  
	while (BpsSharedBuffer[0] != '\x00')  
		Sleep(20);
}

void HandleBreakpoints(char *old_buffer)
{
	if (BpsSharedBuffer[0] == 'a')
	{
		//dprintf("Received a request to add bp %p  \n", *(ULONG64*)(breakpoints_shared_buffer + 1));
		RespondToBreakpointRequest(g_IDBG.AddBreakPoint(*(ULONG64*)(BpsSharedBuffer + 1)));
	}

	else if (BpsSharedBuffer[0] == 'r')
	{
		//dprintf("Received a request to remove a bp \n");
		bool remove_succeded = g_IDBG.RemoveBreakpoint(*(ULONG64*)(BpsSharedBuffer + 1));
		RespondToBreakpointRequest(remove_succeded);
	}


	memset(BpsSharedBuffer, 0, BUF_SIZE);
}

void MainThread()
{
	if (!CreateSharedBuffers())
	{
		dprintf("Failed to create the shared buffers \n");
		return;
	}

	
	char old_buffer[BUF_SIZE];
	memset(old_buffer, 0, BUF_SIZE);


	while (!ShouldUnload)
	{
		HandleBreakpoints(old_buffer);
		Sleep(50);
	}
}



VOID WinDbgExtensionDllInit(
	PWINDBG_EXTENSION_APIS lpExtensionApis,
	USHORT MajorVersion,
	USHORT MinorVersion
)
{
	ExtensionApis = *lpExtensionApis;
	SavedMajorVersion = MajorVersion;
	SavedMinorVersion = MinorVersion;
}



LPEXT_API_VERSION ExtensionApiVersion(void)
{
	return &g_ExtApiVersion;
}

DECLARE_API(sync_with)
{
	if (g_IDBG.IsInUse())
	{
		dprintf("Already in use");
		return;
	}

	if (g_IDBG.Init(args))
		dprintf("IDBG Loaded successfully ");

	hMainThread = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)MainThread, 0, 0, 0); //todo error check
}

DECLARE_API(unload_idbg)
{
	int timeout = 0;

	ShouldUnload = true;
	WaitForSingleObject(hMainThread, INFINITE);
	CloseHandle(hMainThread);

	BpsSharedBuffer[0] = 's';
	
	dprintf("Click on the ida view screen to unload idbg \n");
	while (BpsSharedBuffer[0] == 's' && timeout < 5000)
	{
		Sleep(50);
		timeout++;
	}

	
	if (hBpsFile)
	{
		CloseHandle(hBpsFile);
		if(BpsSharedBuffer)
			UnmapViewOfFile(BpsSharedBuffer);
	}
}





