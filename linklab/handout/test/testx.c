#include <stdlib.h>

int main(void)
{
  void *a;
   void *b;
  a = realloc((void*) 0x10101010, 1024);
   free(a);
    b = realloc(a,1024);

  return 0;
}
