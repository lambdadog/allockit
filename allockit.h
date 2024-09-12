/* allockit.h - implementation of passable allocators in C

   FLAGS
     ALLOCKIT_ALIGNOF (default: alignof)
     ALLOCKIT_SIZE_T (default: size_t)
       Macros to allow the use of AllocKit without a C standard
       library. If ALLOCKIT_ALIGNOF is not defined, stdalign.h is
       used. If ALLOCKIT_SIZE_T is not defined, stddef.h is used.

   USAGE

     Using AllocKit Allocators

       Using an AllocKit allocator is quite simple. First you will
       need to initialize the allocator. This is done via
       allocator-specific types and functions, so for this example
       we'll use a theoretical allocator ("my_alloc.h").

         #include "allockit.h"
         #include "my_alloc.h"

         void do_work(AkAlloc *);

         int
         main(int argc, char **argc)
         {

       For this theoretical allocator, we'll use `my_alloc_init` for
       initialization. For other allocators, it's possible this
       function will be different (potentially using "create"
       semantics rather than "init" semantics), or it may even just
       have a global, constant allocator available for use.

       It's recommended for allocators that use the init function
       pattern that you 0-initialize the allocator so that forgetting
       to call the init function simply causes an easily-detectable
       null pointer dereference when you use it, rather than reading
       whatever bytes were previously in that stack space as a pointer
       and potentially causing more confusing errors.

           MyAlloc my_allocator = {0};
           my_alloc_init(&my_allocator);

       Then we can use the allocator via `ak_alloc`, passing it
       `&my_allocator.alloc`. This pointer (of type `AkAlloc *` is
       also what you'll want functions that allow usage of an
       arbitrary allocator to take as an argument.

           int *heap_list = ak_alloc(&my_allocator.alloc, int, 10)
           do_work(&my_allocator.alloc);

       To resize our allocation (similar to realloc) we'll use
       `ak_resize`. Since resizing an allocation won't always succeed,
       we need to check its return result to ensure the operation was
       successful. In this trivial example we'll just exit the program
       with an error code.

           if (!ak_resize(&my_allocator.alloc, int, 20))
             return 1;

       Finally, to free our allocation we use `ak_free`.

           ak_free(&my_allocator.alloc, heap_list);

       After we're done using our allocator, it may have resources it
       cannot yet free (depending on the allocator), so some
       allocators may provide a "deinit" or "destroy" function. If the
       allocator you're using has one of these, it's important to call
       it to avoid space leaks.

           my_alloc_deinit(&my_allocator);

           return 0;
         }

     Writing AllocKit Allocators

       Writing an AllocKit allocator is a touch more complicated than
       using one, but I've endeavored to make it as simple as
       possible. There are currently no examples, but that will change
       soon.

       IMPORTANT: It is the responsibility of the allocator to ensure
       thread-safety, AllocKit provides no guarantees in this regard.

       An allocator in AllocKit is defined by the `AkAlloc` type,
       which can be seen below (with some simplification):

         typedef struct AkAlloc {
           void *(*alloc)(AkAlloc *, size_t, size_t, size_t);
           int (*resize)(AkAlloc *, void *, size_t, size_t, size_t);
           void (*free)(AkAlloc *, void *);
         } AkAlloc;

       To create an allocator, you must simply implement this
       struct. Of course, there are several notes and tricks to keep
       in mind.

       The trio of `size_t`s in the arguments of `alloc` and `resize`
       are the size, alignment, and count of the allocation requested,
       respectively.

       The void pointer argument in `resize` and `free` is the address
       of the existing allocation.

       The `AkAlloc *` argument is included to make a simple trick
       possible: It's common to need to hold state or some sort of
       userdata for an allocator at runtime. By defining a struct for
       your allocator with `AkAlloc` (NOT a pointer to AkAlloc,
       embedding the actual struct) as the first member, you can
       simply cast the `AkAlloc *` pointer to a pointer of your
       allocator's struct, since it's at address offset 0.

         struct MyAlloc {
           AkAlloc alloc;
           int counter;
         };

         static
         void
         myFree(AkAlloc *alloc, void *addr)
         {
           struct MyAlloc *my_alloc = (struct MyAlloc *)alloc;

           ...
         }

       It is expected, although not required, for you to implement
       both an "init" (or "create") and "deinit" or (destroy) function
       for your allocator. This is an exercise left up to the reader.

       Let's go into more detail on each of those functions.

       IMPORTANT: None of these function pointers may be left NULL
       after calling your "init" function. It is EXPLICITLY an error
       on the implementor's part if one is left so, and will lead to
       undefined behavior when the null pointer is dereferenced at
       runtime. If your allocator does not have an implementation for
       one of these functions, you MUST still provide a function
       pointer, even if it is a no-op or simply panics.

         void *(*alloc)(AkAlloc *, size_t, size_t, size_t);

         static
         void *
         myAlloc(AkAlloc *alloc, size_t size, size_t align, size_t count)
         { ... }

       `alloc` is the only function of an AllocKit allocator which
       cannot reasonably have a "no-op" implementation. While there
       are allocators that cannot free data (or cannot always free
       data) and those incapable of resizing, an allocator must be
       able to allocate data to actually do its job.

       When implementing `alloc`, you will be expected to return NULL
       in the case of an error, and otherwise return a pointer to the
       start of a chunk of memory that is `size * count` bytes long,
       aligned to a multiple of `align`. While you may return NULL (or
       even panic, if you wish) for non-accepted alignments, you
       should never ignore the value of `align`, as the returned data
       will be expected to be aligned and may cause significant errors
       if it is not.

         int (*resize)(AkAlloc *, void *, size_t, size_t, size_t);

         static
         int
         myResize(AkAlloc *alloc, void *addr, size_t size, size_t align, size_t count)
         { ... }

       `resize` attempts to resize an existing allocation, returning 1
       on success and 0 on failure. The no-op implementation of resize
       is quite simple, just returning 0 immediately.

       When implementing `resize`, it's recommended to add a debug
       check to ensure that the alignment matches the existing
       allocation. This is fairly simple with <stdassert.h>:

         assert((uintptr_t)addr % align == 0)

       `resize` should attempt to resize the allocation at `addr` to
       the size `size * count`, maintaining the alignment `align`. If
       the resize is not possible, no action should be taken and
       `resize` should return 0. Otherwise, perform the resize and
       return 1. This function may not change the address allocated
       at, only the size, which is why it's the caller's
       responsibility to check the return value.

         void (*free)(AkAlloc *, void *);

         static
         void
         myFree(AkAlloc *alloc, void *addr)
         { ... }

       `free` attempts to free the memory at `addr`. Some allocators
       are incapable of freeing memory in some or all cases. For these
       allocators, free should just no-op, returning immediately.

 */

#ifndef ALLOCKIT_H_DEFS
#define ALLOCKIT_H_DEFS

#ifndef ALLOCKIT_ALIGNOF
#  include <stdalign.h>
#  define ALLOCKIT_ALIGNOF alignof
#endif  /* !ALLOCKIT_ALIGNOF */

#ifndef ALLOCKIT_SIZE_T
#  include <stddef.h>
#  define ALLOCKIT_SIZE_T size_t
#endif  /* !ALLOCKIT_SIZE_T */

typedef struct AkAlloc {
  void *(*alloc)(struct AkAlloc *,
                 ALLOCKIT_SIZE_T, ALLOCKIT_SIZE_T, ALLOCKIT_SIZE_T);
  int (*resize)(struct AkAlloc *,
                void *,
                ALLOCKIT_SIZE_T, ALLOCKIT_SIZE_T, ALLOCKIT_SIZE_T);
  void (*free)(struct AkAlloc *,
               void *);
} AkAlloc;

#define ak_alloc(pAlloc, Size, Align, Count) \
  (((pAlloc)->alloc)((pAlloc), Size, Align, Count))
#define ak_alloc(pAlloc, T, Count) \
  ak_alloc(pAlloc, sizeof(T), ALLOCKIT_ALIGNOF(T), Count)

#define ak_resize(pAlloc, Addr, Size, Align, Count) \
  (((Alloc)->resize)((pAlloc), Addr, Size, Align, Count))
#define ak_resize(pAlloc, Addr, T, Count) \
  ak_resize(pAlloc, Addr, sizeof(T), ALLOCKIT_ALIGNOF(T), Count)

#define ak_free(pAlloc, Addr) \
  (((Alloc)->free)((pAlloc), Addr))

#endif  /* !ALLOCKIT_H_DEFS */

/*
  This software is available under 2 licenses, choose whichever you
  prefer:

  ALTERNATIVE A - Public Domain (www.unlicense.org)
    This is free and unencumbered software released into the public
    domain.

    Anyone is free to copy, modify, publish, use, compile, sell, or
    distribute this software, either in source code form or as a
    compiled binary, for any purpose, commercial or non-commercial,
    and by any means.

    In jurisdictions that recognize copyright laws, the author or
    authors of this software dedicate any and all copyright interest
    in the software to the public domain. We make this dedication for
    the benefit of the public at large and to the detriment of our
    heirs and successors. We intend this dedication to be an overt act
    of relinquishment in perpetuity of all present and future rights
    to this software under copyright law.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
    NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY
    CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
    CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
    WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

    For more information, please refer to <http://unlicense.org/>

  ALTERNATIVE B - MIT License
    Copyright (c) 2024 lambdadog

    Permission is hereby granted, free of charge, to any person
    obtaining a copy of this software and associated documentation
    files (the "Software"), to deal in the Software without
    restriction, including without limitation the rights to use, copy,
    modify, merge, publish, distribute, sublicense, and/or sell copies
    of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be
    included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
    HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
    WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
 */
