/* Self */

#include "version.h"

char const version[] = 
#ifdef VERSION
    VERSION;
#else
    "1.4.1-"
    "UNKNOWN-Development-Only!\n"
    "You are seeing this version message because the distributor did not build the binary in the git work tree nor pass the version via VERSION= when running make. This version will NOT be supported because the maintainer won't know which exact version you're using!";
#endif