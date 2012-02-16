#cmakedefine YAJL_FOUND 1
#define HAVE_YAJL YAJL_FOUND

#if YAJL_FOUND
#include <yajl/yajl_common.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_parse.h>
#endif