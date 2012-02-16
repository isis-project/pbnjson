/* @@@LICENSE
*
*      Copyright (c) 2012 Hewlett-Packard Development Company, L.P.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
LICENSE@@@ */

#ifndef QBACKTRACE_H_
#define QBACKTRACE_H_

#include <pjson_execinfo.h>

#if __cplusplus

#include <QStringList>
#include <QtDebug>

/** Obtain a backtrace and return as a QStringList.
    This is not well-optimised, requires compilation with "-rdynamic" on linux
    and doesn't do a great job of demangling the symbol names. It is sufficient
    though to work out call trace. */
static inline QStringList getBackTrace()
{
    //now get the backtrace of the code at this point
    //(we can only do this if we have 'execinfo.h'
#if HAVE_EXECINFO_H

    //create a void* array to hold the function addresses. We will only go at most 25 deep
    const int maxdepth(25);

    void *func_addresses[maxdepth];
    int nfuncs = backtrace(func_addresses, maxdepth);

    //now get the function names associated with these symbols. This should work for elf
    //binaries, though additional linker options may need to have been called
    //(e.g. -rdynamic for GNU ld. See the glibc documentation for 'backtrace')
    char **symbols = backtrace_symbols(func_addresses, nfuncs);

    //save all of the function names onto the QStringList....
    //(note that none of this will work if we have run out of memory)
    QStringList ret;

    for (int i=0; i<nfuncs; i++)
    {
        int stat;
#if DEMANGLE_CPP
        char *demangled = __cxa_demangle(symbols[i],0,0,&stat);
        if (demangled && stat == 0)
        {
            ret.append(demangled);
            free(demangled);
        }
        else
#else
#error "Not demangling C++ backtrace"
#endif
            //append the raw symbol
            ret.append(symbols[i]);
    }

    //we now need to release the memory of the symbols array. Since it was allocated using
    //malloc, we must release it using 'free'
    free(symbols);

    return ret;

#else
#warning "Not doing a backtrace"
    return QStringList( QObject::tr(
                "Backtrace is not available without execinfo.h")
                      );
#endif

}

#endif

#endif /* QBACKTRACE_H_ */
