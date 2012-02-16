set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_VERSION 1)
set(CMAKE_SYSTEM_PROCESSOR arm-none-linux-gnueabi)

#if (NOT HOME_CROSS)
#	message(FATAL_ERROR "HOME_CROSS variable not provided")
#else (NOT HOME_CROSS)
#	message("Using cross compiler at ${HOME_CROSS}")
#endif (NOT HOME_CROSS)

#if (NOT HOME_BUILD)
#	message(FATAL_ERROR "HOME_BUILD variable not provided")
#else (NOT HOME_BUILD)
#	message("Using OE build directory at ${HOME_CROSS}")
#endif (NOT HOME_BUILD)

set(CMAKE_C_COMPILER ${HOME_CROSS}/arm-none-linux-gnueabi-gcc)
#CXX not necessary in addition to C for gnu toolchain
#set(CMAKE_CXX_COMPILER arm-none-linux-gnueabi-g++)


set(ENV{PKG_CONFIG_PATH} $ENV{PKG_CONFIG_PATH} ${HOME_BUILD}/staging/arm-none-linux-gnueabi/lib/pkgconfig/)

set(CMAKE_FIND_ROOT_PATH ${HOME_CROSS}/../ ${HOME_BUILD}/staging/arm-none-linux-gnueabi ${HOME_BUILD}/staging/i686-linux/ ${HOME_BUILD}/staging)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

