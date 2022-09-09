cmake_minimum_required (VERSION 3.1)
# Sometimes, C11 threads are implemented as part of pthreads library.
find_package (Threads)
# On FreeBDS, C11 threads are implemented as part of libstdthreads
find_library(STDTHREADS_LIB stdthreads)
if (STDTHREADS_LIB)
  set (LIBS ${LIBS} ${STDTHREADS_LIB})
endif()

add_executable (mainsfrequency-display mainsfrequency-display.c ttyutil.h ttyutil.c)

set (CMAKE_C_STANDARD 11)
#target_link_libraries (c11_threads ${CMAKE_THREAD_LIBS_INIT} ${LIBS})
