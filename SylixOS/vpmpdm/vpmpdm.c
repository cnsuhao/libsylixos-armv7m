/*
 * SylixOS(TM)  LW : long wing
 * Copyright All Rights Reserved
 *
 * MODULE PRIVATE DYNAMIC MEMORY IN VPROCESS PATCH
 * this file is support current module malloc/free use his own heap in a vprocess.
 * 
 * Author: Han.hui <sylixos@gmail.com>
 */

#define  __SYLIXOS_STDIO
#define  __SYLIXOS_KERNEL /* need some kernel function */
#include <sys/cdefs.h>
#include <unistd.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <dlfcn.h>
#include <errno.h>
#include <assert.h>
#include <wchar.h>

#ifdef LW_CFG_CPU_ARCH_ARM
#include "./loader/include/loader_lib.h" /* need _Unwind_Ptr */
#endif /* LW_CFG_CPU_ARCH_ARM */

#define __VP_PATCH_VERSION      "1.3.0" /* vp patch version */

#ifdef __GNUC__
#if __GNUC__ < 2  || (__GNUC__ == 2 && __GNUC_MINOR__ < 5)
#error must use GCC include "__attribute__((...))" function.
#endif /*  __GNUC__ < 2  || ...        */
#endif /*  __GNUC__                    */

#if (LW_CFG_DEVICE_EN > 0) && (LW_CFG_FIO_LIB_EN > 0)
#define __LIB_PERROR(s) perror(s)
#else
#define __LIB_PERROR(s)
#endif /*  (LW_CFG_DEVICE_EN > 0)...   */

#if LW_CFG_USER_HEAP_SAFETY
#define __HEAP_CHECK    TRUE
#else
#define __HEAP_CHECK    FALSE
#endif /* LW_CFG_USER_HEAP_SAFETY */

/*
 *  atexit node
 */
typedef struct exit_node {
    struct exit_node *next; /* next atexit node */
    void (*func)(void); /* atexit function */
} exit_node;

/*
 *  vprocess context
 */
#define MAX_MEM_BLKS 16

typedef struct vp_ctx {
    LW_OBJECT_HANDLE  locker; /* vpmpdm lock */
    LW_CLASS_HEAP  heap; /* this private heap */
    size_t  blksize; /* vmm allocate size pre time */
    void  *vmem[MAX_MEM_BLKS]; /* vmm memory block pointer */
    void  *proc; /* this process */
    int allc_en; /* allocate memory is emable */
    exit_node *atexit_header; /* atexit node list */
} vp_ctx;

static vp_ctx ctx = {
    .blksize = 8192, /* default vmm block memory size */
};

/*
 * lock & unlock vpmpdm
 */
void __vp_patch_lock (void)
{
    API_SemaphoreMPend(ctx.locker, LW_OPTION_WAIT_INFINITE);
}

void __vp_patch_unlock (void)
{
    API_SemaphoreMPost(ctx.locker);
}

 
/*
 *  libgcc support
 */
#ifdef LW_CFG_CPU_ARCH_ARM
/* For a given PC, find the .so that it belongs to.
 * Returns the base address of the .ARM.exidx section
 * for that .so, and the number of 8-byte entries
 * in that section (via *pcount).
 *
 * libgcc declares __gnu_Unwind_Find_exidx() as a weak symbol, with
 * the expectation that libc will define it and call through to
 * a differently-named function in the dynamic linker.
 */
_Unwind_Ptr __gnu_Unwind_Find_exidx (_Unwind_Ptr pc, int *pcount)
{
    return dl_unwind_find_exidx(pc, pcount, ctx.proc);
}
#endif /* LW_CFG_CPU_ARCH_ARM */

/*
 *  get vp patch version
 */
char *__vp_patch_version (void *pvproc)
{
    (void)pvproc;

    return  (__VP_PATCH_VERSION);
}

/*
 *  get vp patch heap
 */
void *__vp_patch_heap (void *pvproc)
{
    return  ((void *)&ctx.heap);
}

/*
 *  get vp vmem heap
 */
int __vp_patch_vmem (void *pvproc, void **pvmem, int size)
{
    int  i, cnt = 0;
    
    for (i = 0; i < MAX_MEM_BLKS; i++) {
        if (cnt >= size) {
            break;
        }
        if (ctx.vmem[i]) {
            pvmem[cnt] = ctx.vmem[i];
            cnt++;
        }
    }
    
    return  (cnt);
}

/*
 *  init vp patch process 
 *  (sylixos load this module will call this function first)
 */
void __vp_patch_ctor (void *pvproc)
{
    char buf[12] = "8192"; /* default 8192 pages */

    if (ctx.proc) {
        return;
    }

    ctx.proc = pvproc; /* save this process handle, dlopen will use this */

    API_TShellVarGetRt("SO_MEM_PAGES", buf, sizeof(buf)); /* must not use getenv() */
    
    ctx.blksize = (unsigned long)lib_atol(buf);
    if (ctx.blksize < 0) {
        return;
    }
    ctx.blksize *= LW_CFG_VMM_PAGE_SIZE;

    ctx.locker = API_SemaphoreMCreate("vp_lock", LW_PRIO_DEF_CEILING,
                                      LW_OPTION_INHERIT_PRIORITY | 
                                      LW_OPTION_OBJECT_GLOBAL, LW_NULL);
    if (!ctx.locker) {
        fprintf(stderr, "WARNING: ctx.locker create error!\r");
    }

#if LW_CFG_VMM_EN > 0
    ctx.vmem[0] = vmmMallocArea(ctx.blksize, NULL, NULL);
    if (ctx.vmem[0]) {
        lib_itoa(getpid(), ctx.heap.HEAP_cHeapName, 10);
        _HeapCtor(&ctx.heap, ctx.vmem[0], ctx.blksize);
        ctx.allc_en = 1;
    }
#endif /* LW_CFG_VMM_EN > 0 */
}

/*
 *  deinit vp patch process 
 *  (sylixos unload this module will call this function last)
 */
void __vp_patch_dtor (void *pvproc)
{
#if LW_CFG_VMM_EN > 0
    int i;
    if (ctx.allc_en) {
        _HeapDtor(&ctx.heap, FALSE);
        for (i = 0; i < MAX_MEM_BLKS; i++) {
            if (ctx.vmem[i]) {
                vmmFreeArea(ctx.vmem[i]); /* free all module private memory area */
                ctx.vmem[i] = NULL;
            }
        }
    }
#endif /* LW_CFG_VMM_EN > 0 */

    if (ctx.locker) {
        API_SemaphoreMDelete(&ctx.locker);
    }

    ctx.proc = NULL;
}

/*
 *  __vp_patch_aerun
 *  (sylixos unload this module will call this function before __vp_patch_dtor)
 */
void __vp_patch_aerun (void)
{
    exit_node *temp;
    exit_node *next;
    
    temp = ctx.atexit_header;
    while (temp) {
        temp->func();
        next = temp->next;
        lib_free(temp);
        temp = next;
    }
    ctx.atexit_header = NULL;
}

/*
 *  atexit
 */
int atexit (void (*func)(void))
{
    exit_node *node;

    if (!func) {
        errno = EINVAL;
        return  (-1);
    }

    node = (exit_node *)lib_malloc(sizeof(exit_node));
    if (!node) {
        errno = ENOMEM;
        return  (-1);
    }

    node->func = func;

    __KERNEL_ENTER();
    node->next = ctx.atexit_header;
    ctx.atexit_header = node;
    __KERNEL_EXIT();

    return  (0);
}

#if LW_CFG_VMM_EN > 0
/*
 *  pre-alloc physical pages (use page fault mechanism)
 */
static void  pre_alloc_phy (const void *pmem, size_t nbytes)
{
    unsigned long  algin = (unsigned long)pmem;
    unsigned long  end   = algin + nbytes - 1;
    volatile char  temp;
    
    if (!pmem || !nbytes) {
        return;
    }

    temp = *(char *)algin;

    algin |= (LW_CFG_VMM_PAGE_SIZE - 1);
    algin += 1;

    while (algin <= end) {
        temp = *(char *)algin;
        algin += LW_CFG_VMM_PAGE_SIZE;
    }
    
    (void)temp; /* no warning for this variable */
}

/*
 *  add new memory to heap
 */
static void  heap_mem_extern (void)
{
    int i;
    volatile char  temp;

    if (ctx.allc_en) {
        __vp_patch_lock();
        for (i = 0; i < MAX_MEM_BLKS; i++) {
            if (ctx.vmem[i] == NULL) {
                ctx.vmem[i] = vmmMallocArea(ctx.blksize, NULL, NULL);
                if (ctx.vmem[i]) {
                    temp = *(char *)ctx.vmem[i];    /* vmm alloc physical page */
                    _HeapAddMemory(&ctx.heap, ctx.vmem[i], ctx.blksize);
                }
                break;
            }
        }
        __vp_patch_unlock();
    }

    (void)temp; /* no warning for this variable */
}

/*
 *  lib_malloc
 */
void *lib_malloc (size_t  nbytes)
{
    int mextern = 0;
    void *pmem;
    
    nbytes = (nbytes) ? nbytes : 1;

    if (ctx.allc_en) {
__re_try:
        pmem = _HeapAllocate(&ctx.heap, nbytes, __func__);
        if (pmem) {
            pre_alloc_phy(pmem, nbytes);
        } else {
            if (mextern) {
                errno = ENOMEM;
            } else {
                mextern = 1;
                heap_mem_extern();
                goto    __re_try;
            }
        }
        return  (pmem);
    }

    return  (NULL);
}
void *malloc (size_t  nbytes)
{
    return  (lib_malloc(nbytes));
}

/*
 *  lib_xmalloc
 */
void *lib_xmalloc (size_t  nbytes)
{
    void  *ptr = lib_malloc(nbytes);
    
    if (ptr == NULL) {
        __LIB_PERROR("lib_xmalloc() process not enough memory");
        exit(0);
    }
    
    return  (ptr);
}
void *xmalloc (size_t  nbytes)
{
    return  (lib_xmalloc(nbytes));
}

/*
 *  lib_mallocalign
 */
void *lib_mallocalign (size_t  nbytes, size_t align)
{
    int mextern = 0;
    void *pmem;
    
    if (align & (align - 1)) {
        errno = EINVAL;
        return  (NULL);
    }
    
    nbytes = (nbytes) ? nbytes : 1;

    if (ctx.allc_en) {
__re_try:
        pmem = _HeapAllocateAlign(&ctx.heap, nbytes, align, __func__);
        if (pmem) {
            pre_alloc_phy(pmem, nbytes);
        } else {
            if (mextern) {
                errno = ENOMEM;
            } else {
                mextern = 1;
                heap_mem_extern();
                goto    __re_try;
            }
        }
        return  (pmem);
    }

    return  (NULL);
}

void *lib_xmallocalign (size_t  nbytes, size_t align)
{
    void  *ptr = lib_mallocalign(nbytes, align);
    
    if (ptr == NULL) {
        __LIB_PERROR("lib_xmallocalign() process not enough memory");
        exit(0);
    }
    
    return  (ptr);
}

void *mallocalign (size_t  nbytes, size_t align)
{
    return  (lib_mallocalign(nbytes, align));
}

void *xmallocalign (size_t  nbytes, size_t align)
{
    return  (lib_xmallocalign(nbytes, align));
}

/*
 * aligned malloc
 */
void *aligned_malloc (size_t  bytes, size_t alignment)
{
    return  (lib_mallocalign(bytes, alignment));
}

void aligned_free (void *p)
{
    lib_free(p);
}

/*
 *  lib_memalign
 */
void *lib_memalign (size_t align, size_t  nbytes)
{
    return  (lib_mallocalign(nbytes, align));
}

void *lib_xmemalign (size_t align, size_t  nbytes)
{
    void  *ptr = lib_memalign(align, nbytes);
    
    if (ptr == NULL) {
        __LIB_PERROR("lib_xmemalign() process not enough memory");
        exit(0);
    }
    
    return  (ptr);
}

void *memalign (size_t align, size_t  nbytes)
{
    return  (lib_memalign(align, nbytes));
}

void *xmemalign (size_t align, size_t  nbytes)
{
    return  (lib_xmemalign(align, nbytes));
}

/*
 *  lib_free
 */
void lib_free (void *ptr)
{
    if (ptr && ctx.allc_en) {
        _HeapFree(&ctx.heap, ptr, __HEAP_CHECK, __func__);
    }
}

void free (void *ptr)
{
    lib_free(ptr);
}

/*
 *  lib_calloc
 */
void *lib_calloc (size_t  num, size_t  nbytes)
{
    size_t total = num * nbytes;
    void *pmem;
    
    pmem = lib_malloc(total);
    if (pmem) {
        lib_bzero(pmem, total);
    }
    
    return  (pmem);
}

void *calloc (size_t  num, size_t  nbytes)
{
    return  (lib_calloc(num, nbytes));
}

/*
 *  lib_xcalloc
 */
void *lib_xcalloc (size_t  num, size_t  nbytes)
{
    void  *ptr = lib_calloc(num, nbytes);
    
    if (ptr == NULL) {
        __LIB_PERROR("lib_xcalloc() process not enough memory");
        exit(0);
    }
    
    return  (ptr);
}
void *xcalloc (size_t  num, size_t  nbytes)
{
    return  (lib_xcalloc(num, nbytes));
}

/*
 *  lib_realloc
 */
void *lib_realloc (void *ptr, size_t  new_size)
{
    int mextern = 0;
    void *pmem;
    
    if (ctx.allc_en) {
__re_try:
        pmem = _HeapRealloc(&ctx.heap, ptr, new_size, __HEAP_CHECK, __func__);
        if (pmem) {
            pre_alloc_phy(pmem, new_size);
        } else {
            if (mextern) {
                errno = ENOMEM;
            } else {
                mextern = 1;
                heap_mem_extern();
                goto    __re_try;
            }
        }
        return  (pmem);
    }
    
    return  (NULL);
}

void *realloc (void *ptr, size_t  new_size)
{
    return  (lib_realloc(ptr, new_size));
}

/*
 *  lib_xrealloc
 */
void *lib_xrealloc (void *ptr, size_t  new_size)
{
    void  *new_ptr = lib_realloc(ptr, new_size);
    
    if ((new_ptr == NULL) && new_size) {
        __LIB_PERROR("lib_xrealloc() process not enough memory");
        exit(0);
    }
    
    return  (new_ptr);
}

void *xrealloc (void *ptr, size_t  new_size)
{
    return  (lib_xrealloc(ptr, new_size));
}

/*
 *  lib_posix_memalign
 */
int  lib_posix_memalign (void **memptr, size_t align, size_t size)
{
    if (memptr == NULL) {
        errno = EINVAL;
        return  (EINVAL);
    }

    if (align & (align - 1)) {
        errno = EINVAL;
        return  (EINVAL);
    }

    size = (size) ? size : 1;

    *memptr = lib_mallocalign(size, align);
    if (*memptr) {
        return  (ERROR_NONE);

    } else {
        errno = ENOMEM;
        return  (ENOMEM);
    }
}

int  posix_memalign (void **memptr, size_t align, size_t size)
{
    return  lib_posix_memalign(memptr, align, size);
}

/*
 *  lib_malloc_new
 */
void *lib_malloc_new (size_t  nbytes)
{
    int mextern = 0;
    void *p;

    nbytes = (nbytes) ? nbytes : 1;

    if (ctx.allc_en) {
__re_try:
        p = _HeapAllocate(&ctx.heap, nbytes, "process C++ new");
        if (p) {
            pre_alloc_phy(p, nbytes);
        } else {
            if (mextern) {
                __LIB_PERROR("process C++ new() not enough memory");
                errno = ENOMEM;
            } else {
                mextern = 1;
                heap_mem_extern();
                goto    __re_try;
            }
        }
        return  (p);
    }
    
    return  (NULL);
}

void *malloc_new (size_t  nbytes)
{
    return  (lib_malloc_new(nbytes));
}

/*
 *  lib_strdup
 */
char *lib_strdup (const char *str)
{
    char *mem = NULL;
    size_t size;

    if (str == NULL) {
        return  (NULL);
    }
    
    size = lib_strlen(str);
    mem = (char *)lib_malloc(size + 1);
    if (mem) {
        lib_strcpy(mem, str);
    }
    
    return  (mem);
}

char *strdup (const char *str)
{
    return  (lib_strdup(str));
}

/*
 *  lib_xstrdup
 */
char *lib_xstrdup (const char *str)
{
    char *str_ret = lib_strdup(str);
    
    if (str_ret == NULL) {
        __LIB_PERROR("lib_xstrdup() process not enough memory");
        exit(0);
    }
    
    return  (str_ret);
}

char *xstrdup (const char *str)
{
    return  (lib_xstrdup(str));
}

/*
 *  lib_strndup
 */
char *lib_strndup (const char *str, size_t size)
{
    char *mem;
    char *end;
    
    if (str == NULL) {
        return  (NULL);
    }
    
    end = (char *)lib_memchr(str, 0, size);
    if (end) {
        size = end - str + 1; /* Length + 1 */
    }

    mem = (char *)lib_malloc(size);

    if (size) {
        lib_memcpy(mem, str, size - 1);
        mem[size - 1] = '\0';
    }
    
    return  (mem);
}

char *strndup (const char *str, size_t size)
{
    return  (lib_strndup(str, size));
}

/*
 *  lib_xstrndup
 */
char *lib_xstrndup (const char *str, size_t size)
{
    char *str_ret = lib_strndup(str, size);
    
    if (str_ret == NULL) {
        __LIB_PERROR("lib_xstrndup() process not enough memory");
        exit(0);
    }
    
    return  (str_ret);
}

char *xstrndup (const char *str, size_t size)
{
    return  (lib_xstrndup(str, size));
}

/*
 *  lib_itoa
 */
char *lib_itoa (int value, char *string, int radix)
{
    char tmp[33];
    char *tp = tmp;
    int i;
    unsigned v;
    int sign;
    char *sp;

    if (radix > 36 || radix <= 1)
    {
        errno = EDOM;
        return 0;
    }

    sign = (radix == 10 && value < 0);
    if (sign)
        v = -value;
    else
        v = (unsigned)value;
    while (v || tp == tmp)
    {
        i = v % radix;
        v = v / radix;
        if (i < 10)
            *tp++ = (char)(i+'0');
        else
            *tp++ = (char)(i + 'a' - 10);
    }

    if (string == 0)
        string = (char *)lib_malloc((tp-tmp)+sign+1);
    sp = string;

    if (sign)
        *sp++ = '-';
    while (tp > tmp)
        *sp++ = *--tp;
    *sp = 0;
    
    return string;
}

char *itoa (int value, char *string, int radix)
{
    return  (lib_itoa(value, string, radix));
}

/*
 * wcsdup
 */
#ifdef __GNUC__
__weak_reference(wcsdup,_wcsdup);
#endif

wchar_t *
wcsdup(const wchar_t *str)
{
    wchar_t *copy;
    size_t len;

    assert(str != NULL);

    len = wcslen(str) + 1;
    copy = malloc(len * sizeof (wchar_t));

    if (!copy)
        return NULL;

    return wmemcpy(copy, str, len);
}

/*
 * getpass
 */
char *getpass (const char *prompt)
{
    static char cPass[PASS_MAX + 1];
    
    return getpass_r(prompt, cPass, sizeof(cPass));
}

#endif /* LW_CFG_VMM_EN > 0 */
