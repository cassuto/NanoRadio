#include "portable.h"

#if ENABLE(BSS_ALIGN)

char unalChar(char const *adr);
short unalShort(short const *adr);

#else

#define unalChar(x) (*(x))
#define unalShort(x) (*(x))

#endif
