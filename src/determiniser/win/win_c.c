#define _WIN32_WINNT 0x0600

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <windows.h>
#include <excpt.h>

#include "offsets.h"
#include "remote_loader.h"
#include "x86_common.h"
#include "common.h"


static void single_step_handler (PCONTEXT ContextRecord)
{
   uintptr_t * gregs = (uintptr_t *) ContextRecord;

   // Run single step handler
   x86_trap_handler (gregs, 1);

   // Completed the single step handler, go back to normal execution
   // Breakpoint with EAX = 0x102 and EBX = pointer to updated context
   x86_bp_trap (COMPLETED_SINGLE_STEP_HANDLER, gregs);
}

void x86_make_text_writable (uintptr_t minAddress, uintptr_t maxAddress)
{
   MEMORY_BASIC_INFORMATION mbi;

   if (!VirtualProtect ((void *) minAddress, maxAddress - minAddress,
         PAGE_EXECUTE_READWRITE, &mbi.Protect)) {
      x86_bp_trap (FAILED_MEMORY_PERMISSIONS, NULL);
   }
}

void x86_make_text_noexec (uintptr_t minAddress, uintptr_t maxAddress)
{
   MEMORY_BASIC_INFORMATION mbi;

   if (!VirtualProtect ((void *) minAddress, maxAddress - minAddress,
         PAGE_NOACCESS, &mbi.Protect)) {
      x86_bp_trap (FAILED_MEMORY_PERMISSIONS, NULL);
   }
}
__declspec(dllexport) void X86DeterminiserStartup (CommStruct * pcs)
{
   SYSTEM_INFO systemInfo;
   MEMORY_BASIC_INFORMATION mbi;
   size_t pageSize, pageMask;

   // Here is the entry point from the RemoteLoader procedure
   // Check internal version first
   x86_check_version (pcs);

   // Discover the bounds of the executable .text segment
   // which is known to contain pcs->startAddress
   // (On Linux the parent process works out the min/max address for the .text section
   // and sets minAddress/maxAddress, and startAddress is not available.)
   GetSystemInfo (&systemInfo);
   pageSize = systemInfo.dwPageSize;
   pageMask = ~ (pageSize - 1);
   pcs->minAddress = (uintptr_t) (((size_t) pcs->startAddress) & pageMask);
   ZeroMemory (&mbi, sizeof (mbi));

   // find minimum page
   while ((VirtualQuery ((void *) (pcs->minAddress - pageSize), &mbi, sizeof (mbi)) != 0)
   && mbi.RegionSize > pageSize) {
      pcs->minAddress -= pageSize;
   }

   // calculate maximum page
   if (VirtualQuery ((void *) pcs->minAddress, &mbi, sizeof (mbi)) == 0) {
      // Error code EAX = 0x104
      x86_bp_trap (FAILED_MEMORY_BOUND_DISCOVERY, NULL);
   }
   pcs->maxAddress = mbi.RegionSize + pcs->minAddress;

   x86_make_text_writable (pcs->minAddress, pcs->maxAddress);
   x86_startup (pcs);

   // Now ready for the user program
   // Breakpoint with EAX = 0x101 and EBX = pointer to single_step_handler
   x86_bp_trap (COMPLETED_LOADER, single_step_handler);
}



