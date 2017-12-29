#include <stdio.h>


int main (void)
{
   const char x[] = "hello world\n";
   int i;

   for (i = 0; x[i] != '\0'; i++) {
      fputc (x[i], stdout);
      fflush (stdout);

      // return with offset is single-stepped, so we can see it
      asm volatile ("push %eax\ncall 1f\njmp 2f\n1: ret $4\n2:");
   }
   return 0;
}

