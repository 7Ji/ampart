/* Self */

#include "version.h"

char const version[] = 
#ifdef VERSION
    VERSION;
#else
    "unknown";
#endif