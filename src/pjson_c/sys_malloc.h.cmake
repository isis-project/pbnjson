#cmakedefine HAVE_MALLOC_H 1

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#else
#include <stdlib.h>
#endif /* HAVE_MALLOC_H */
