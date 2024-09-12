/* allockit_page.h -- utils for managing pages in an os-agnostic manner

   FLAGS
     ALLOCKIT_PAGE_IMPL
       Include the implementation of allockit_page.h's functions. Must
       be included at least once in your project.

     ALLOCKIT_PAGE_STATIC
       Makes function definitions static. Implies ALLOCKIT_PAGE_IMPL.

 */

#ifdef ALLOCKIT_PAGE_STATIC
#  define ALLOCKIT_PAGE_IMPL
#  define AKDEF static
#else
#  define AKDEF extern
#endif

#ifndef ALLOCKIT_PAGE_H_DEFS
#define ALLOCKIT_PAGE_H_DEFS

#include <stddef.h>

typedef struct AkPageChunk {
  void *start;
  size_t count;
} AkPageChunk;

AKDEF size_t ak_page_getPageSize(void);
AKDEF AkPageChunk ak_page_requestPages(size_t, void *, size_t);
AKDEF int ak_page_returnPages(size_t, AkPageChunk *);

#endif  /* !ALLOCKIT_PAGE_H_DEFS */

#undef AKDEF

#if defined(ALLOCKIT_PAGE_IMPL) && !defined(ALLOCKIT_PAGE_H_IMPL)
#define ALLOCKIT_PAGE_H_IMPL

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
#  define ALLOCKIT_PAGE_PLATFORM_WINDOWS
#elif __APPLE__
#  error "allockit_page.h: apple platforms not currently supported"
#elif __linux
#  define ALLOCKIT_PAGE_PLATFORM_LINUX
#else
#  error "allockit_page.h: platform not currently supported"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

#ifdef ALLOCKIT_PAGE_PLATFORM_WINDOWS
#  include <sysinfoapi.h>
#  include <memoryapi.h>
#endif

#ifdef ALLOCKIT_PAGE_PLATFORM_LINUX
#  include <unistd.h>
#  include <sys/mman.h>
#endif

#ifndef SIZE_MAX
#  define SIZE_MAX ((size_t)(-1))
#endif

size_t
ak_page_getPageSize(void)
{
#ifdef ALLOCKIT_PAGE_PLATFORM_WINDOWS
  SYSTEM_INFO sysinfo;
  GetSystemInfo(&sysinfo);

  return (size_t)sysinfo.dwPageSize;
#elif defined(ALLOCKIT_PAGE_PLATFORM_LINUX)
  ssize_t m_page_size = sysconf(_SC_PAGESIZE);

  if (m_page_size <= 0)
    return 0;

  return (size_t)m_page_size;
#endif
}

AkPageChunk
ak_page_requestPages(size_t page_size, void *addr, size_t count)
{
  size_t length;
  AkPageChunk res = {0};

  /* Is there a way we can make this overflow check cheaper? */
  if (count && page_size > SIZE_MAX/count)
    return res;

  length = page_size * count;

#ifdef ALLOCKIT_PAGE_PLATFORM_WINDOWS
  void *m_addr = VirtualAlloc(addr, length,
                              MEM_COMMIT | MEM_RESERVE,
                              PAGE_READWRITE);
#elif defined(ALLOCKIT_PAGE_PLATFORM_LINUX)
  int flags = MAP_ANONYMOUS | MAP_PRIVATE;

  if (addr != NULL)
    flags |= MAP_FIXED;

  void *m_addr = mmap(addr, length, PROT_READ | PROT_WRITE, flags, -1, 0);
  if (m_addr == MAP_FAILED)
    return res;
#endif

  res.start = m_addr;
  res.count = count;
  return res;
}

int
ak_page_returnPages(size_t page_size, AkPageChunk *pages)
{
  int res;

#ifdef ALLOCKIT_PAGE_PLATFORM_WINDOWS
  res = (int)VirtualFree(pages->start, 0, MEM_RELEASE);
#elif defined(ALLOCKIT_PAGE_PLATFORM_LINUX)
  size_t length = page_size * pages->count;

  /* munmap returns -1 on failure, 0 on success */
  res = munmap(pages->start, length) + 1;
#endif

  if (res)
    pages->start = NULL;

  return res;
}

#endif  /* ALLOCKIT_PAGE_IMPL && !ALLOCKIT_PAGE_H_IMPL */
