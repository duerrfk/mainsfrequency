cmake_minimum_required (VERSION 3.1)

project(mainsfrequency)

# Sometimes, C11 threads are implemented as part of pthreads library.
find_package (Threads)
# On FreeBDS, C11 threads are implemented as part of libstdthreads
find_library(STDTHREADS_LIB stdthreads)
if (STDTHREADS_LIB)
  set (LIBS ${LIBS} ${STDTHREADS_LIB})
endif()

include_directories(../../libcrc/include)
link_directories(../../libcrc/lib)

# Required for strptime() in time.h
add_compile_definitions(_XOPEN_SOURCE=700)
# Required for timegm() in time.h 
add_compile_definitions(_DEFAULT_SOURCE)

add_executable(pkt-to-tlv-stream pkt-to-tlv-stream.c tty.h tty.c slip.h slip.c crc.h crc.c tlv.h tlv.c)
add_executable(sink-display sink-display.c tlv.h tlv.c)
add_executable (filter-timewnd filter-timewnd.c tlv.h tlv.c)

set (CMAKE_C_STANDARD 11)
target_link_libraries (pkt-to-tlv-stream libcrc.a)

#target_link_libraries (c11_threads ${CMAKE_THREAD_LIBS_INIT} ${LIBS})
