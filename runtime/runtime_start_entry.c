#include "runtime.h"

#if defined(_MSC_VER)
#    include <stdlib.h>
#endif

extern int start(void);

#if defined(_MSC_VER)
BOLT_NORETURN void bolt_runtime_exit(int code)
{
    exit(code);
}

int mainCRTStartup(void)
{
    int result = start();
    bolt_runtime_exit(result);
}
#endif
