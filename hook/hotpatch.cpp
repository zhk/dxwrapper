/**
* Created from source code found in DxWnd v2.03.60
* https://sourceforge.net/projects/dxwnd/
*
* Updated 2017 by Elisha Riedlinger
*/

// hotpatch compiled system dlls come with Windows XP SP2 or later

// return:
// 0 = patch failed
// 1 = already patched
// addr = address of the original function

#include "cfg.h"

#ifdef USEMINHOOK
#include "MinHook.h"
#endif

void *HotPatch(void *apiproc, const char *apiname, void *hookproc)
{
#ifdef USEMINHOOK
	void *pProc;
	static BOOL DoOnce = TRUE;

	if(DoOnce){
		if (MH_Initialize() != MH_OK) {
			Compat::Log() << "HotPatch: MH_Initialize FAILED";
			// What to do here? No recovery action ...
			return 0;
		}
		DoOnce = FALSE;
	}

#ifdef _DEBUG
	Compat::Log() << "HotPatch: api="<< apiname << " addr=" << std::showbase << std::hex << apiproc << std::dec << std::noshowbase << " hook=" << hookproc;
#endif

	if(!strcmp(apiname, "GetProcAddress")) return 0; // do not mess with this one!

	if (MH_CreateHook(apiproc, hookproc, reinterpret_cast<void**>(&pProc)) != MH_OK){
		Compat::Log() << "HotPatch: MH_CreateHook FAILED";
		return 0;
	}

	if (MH_EnableHook(apiproc) != MH_OK){
		Compat::Log() << "HotPatch: MH_EnableHook FAILED";
		return 0;
	}

#ifdef _DEBUG
	Compat::Log() << "HotPatch: api=" << apiname << " addr=" << std::showbase << std::hex << apiproc << "->" << pProc << std::dec << std::noshowbase << " hook=" << hookproc;
#endif
	return pProc;
#else
	DWORD dwPrevProtect;
	BYTE* patch_address;
	void *orig_address;

#ifdef _DEBUG
	Compat::Log() << "HotPatch: api=" << apiname << " addr=" << std::showbase << std::hex << apiproc << std::dec << std::noshowbase << " hook=" << hookproc;
#endif

	if(!strcmp(apiname, "GetProcAddress")) return 0; // do not mess with this one!

	patch_address = ((BYTE *)apiproc) - 5;
	orig_address = (BYTE *)apiproc + 2;

	// entry point could be at the top of a page? so VirtualProtect first to make sure patch_address is readable
	//if(!VirtualProtect(patch_address, 7, PAGE_EXECUTE_READWRITE, &dwPrevProtect)){
	if(!VirtualProtect(patch_address, 12, PAGE_EXECUTE_WRITECOPY, &dwPrevProtect)){
		Compat::Log() << "HotPatch: access denied. err=" << GetLastError();
		return (void *)0; // access denied
	}

	// some calls (QueryPerformanceCounter) are sort of hot patched already....
	if(!memcmp( "\x90\x90\x90\x90\x90\xEB\x05\x90\x90\x90\x90\x90", patch_address, 12)){
		*patch_address = 0xE9; // jmp (4-byte relative)
		*((DWORD *)(patch_address + 1)) = (DWORD)hookproc - (DWORD)patch_address - 5; // relative address
		*((WORD *)apiproc) = 0xF9EB; // should be atomic write (jmp $-5)
		
		VirtualProtect( patch_address, 12, dwPrevProtect, &dwPrevProtect ); // restore protection
#ifdef _DEBUG
		Compat::Log() << "HotPatch: api=" << apiname << " addr=" << std::showbase << std::hex << apiproc << "->" << orig_address << std::dec << std::noshowbase << " hook=" << hookproc;
#endif
		return orig_address;
	}

	// make sure it is a hotpatchable image... check for 5 nops followed by mov edi,edi
	if(memcmp( "\x90\x90\x90\x90\x90\x8B\xFF", patch_address, 7) && memcmp( "\x90\x90\x90\x90\x90\x89\xFF", patch_address, 7)){
		VirtualProtect( patch_address, 12, dwPrevProtect, &dwPrevProtect ); // restore protection
		// check it wasn't patched already
		if((*patch_address==0xE9) && (*(WORD *)apiproc == 0xF9EB)){
			// should never go through here ...
			Compat::Log() << "HotPatch: patched already\n";
			return (void *)1;
		}
		else{
			Compat::Log() << "HotPatch: not patch aware.";
			return (void *)0; // not hot patch "aware"
		}
	}
	
	*patch_address = 0xE9; // jmp (4-byte relative)
	*((DWORD *)(patch_address + 1)) = (DWORD)hookproc - (DWORD)patch_address - 5; // relative address
	*((WORD *)apiproc) = 0xF9EB; // should be atomic write (jmp $-5)
	
	VirtualProtect( patch_address, 12, dwPrevProtect, &dwPrevProtect ); // restore protection
#ifdef _DEBUG
	Compat::Log() << "HotPatch: api=" << apiname << " addr=" << std::showbase << std::hex << apiproc << "->" << orig_address << std::dec << std::noshowbase << " hook=" << hookproc;
#endif
	return orig_address;
#endif
}