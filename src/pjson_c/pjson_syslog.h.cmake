// If the macro is defined to override output, then
// we disable the other targets

#cmakedefine HAVE_SYSLOG_H 1
#ifndef PJSON_LOG_STDOUT
#cmakedefine HAVE_VSYSLOG 1
#endif
#cmakedefine HAVE_VFPRINTF 1

#if HAVE_SYSLOG_H
#include <syslog.h>
#endif
