#cmakedefine HAVE_PCRE 1
#cmakedefine HAVE_POSIX_REGEXP 1

#if HAVE_PCRE
    #include <pcreposix.h>
#elif HAVE_POSIX_REGEXP
    #ifdef NDEBUG
        #warning "Using POSIX regular expressions - this violates the JSON schema spec"
    #endif 
    #include <regex.h>
#else
    #error "No regular expression support provided - schema requires them"
#endif

#if HAVE_PCRE || HAVE_POSIX_REGEXP
/* 
 * USE_POSIX_REGEXP determines whether or not we use the POSIX API for accessing the
 * regular expression engine.
 */
#define USE_POSIX_REGEXP 1
#endif