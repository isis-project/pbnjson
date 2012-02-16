#cmakedefine HAVE_STRNLEN 1

#include <string.h>

#if !HAVE_STRNLEN
static size_t strnlen(const char *s, size_t n)
{
	const char *p = (const char *)memchr(s, 0, n);
	return (p ? p - s : n);
}
#endif

