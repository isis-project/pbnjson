#cmakedefine CJSON_FOUND 1
#define HAVE_CJSON CJSON_FOUND

#if CJSON_FOUND
#include <json.h>
#endif
