#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>


#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500
#endif

#include <windows.h>
#include <excpt.h>

#include "offsets.h"
#include "remote_loader.h"

void x86_trap_handler (uint32_t * gregs, uint32_t trapno);
int x86_startup (size_t minPage, size_t maxPage, int debugEnabled);

static void single_step_handler (PCONTEXT ContextRecord)
{
   uint32_t * gregs = (uint32_t *) ContextRecord;

   // Run single step handler
   x86_trap_handler (gregs, 1);

   // Completed the single step handler, go back to normal execution
   // Breakpoint with EAX = 0x102 and EBX = pointer to updated context
   asm volatile ("mov %0, %%ebx\nmov $0x102, %%eax\nint3\n"
      : : "r"(gregs) );
}

__declspec(dllexport) void X86DeterminiserStartup (CommStruct * cs)
{
   SYSTEM_INFO systemInfo;
   MEMORY_BASIC_INFORMATION mbi;
   size_t pageSize, pageMask, minPage, maxPage;
   int rc = 0;

   // Here is the entry point from the RemoteLoader procedure
   // Discover the bounds of the executable .text segment
   // which is known to contain cs->startAddress
   GetSystemInfo (&systemInfo);
   pageSize = systemInfo.dwPageSize;
   pageMask = ~ (pageSize - 1);
   minPage = ((size_t) cs->startAddress) & pageMask;
   ZeroMemory (&mbi, sizeof (mbi));

   // find minimum page
   while ((VirtualQuery ((void *) (minPage - pageSize), &mbi, sizeof (mbi)) != 0)
   && mbi.RegionSize > pageSize) {
      minPage -= pageSize;
   }

   // calculate maximum page
   if (VirtualQuery ((void *) minPage, &mbi, sizeof (mbi)) == 0) {
      // Error code EAX = 0x104
      rc = 0x104;
      goto error;
   }
   maxPage = mbi.RegionSize + minPage;

   // make this memory region writable
   if (!VirtualProtect ((void *) minPage, maxPage - minPage,
         PAGE_EXECUTE_READWRITE, &mbi.Protect)) {
      // Error code EAX = 0x103
      rc = 0x103;
      goto error;
   }

   rc = x86_startup (minPage, maxPage, cs->debugEnabled);
   if (rc != 0) {
      // Error code EAX = rc
      goto error;
   }

   // Now ready for the user program
   // Breakpoint with EAX = 0x101 and EBX = pointer to single_step_handler
   asm volatile ("mov %0, %%ebx\nmov $0x101, %%eax\nint3\n"
      : : "r"(single_step_handler) );

error:
   // Error handler - send error code to loader process
   asm volatile ("mov %0, %%eax\nint3\n" : : "r"(rc) );
}


