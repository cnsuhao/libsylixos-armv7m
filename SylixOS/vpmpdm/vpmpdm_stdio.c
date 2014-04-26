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
#include <stdio.h>
#include <string.h>

/*
 *  vasprintf() in kernel function pointer
 */
static int (*__pfunc_vasprintf)() = NULL;

/*
 *  init kernel function pointer
 */
static int __pfunc_init (void)
{
    if (__pfunc_vasprintf == NULL) {
        __pfunc_vasprintf = (int (*)())symbolFind("vasprintf", LW_SYMBOL_FLAG_XEN);
        if (__pfunc_vasprintf == NULL) {
            return  (-1);
        }
    }

    return  (0);
}

/*
 *  vasprintf in process mode (use process memory)
 */
int  vasprintf (char **str, const char *fmt, _BSD_VA_LIST_ ap)
{
    char    *kstr;
    int      ret;

    if (!str || !fmt) {
	    return  (0);
	}

    if (__pfunc_init() < 0) {
        return  (-1);
    }

    ret = __pfunc_vasprintf(&kstr, fmt, ap);
    if (ret < 0) {
        *str = NULL;
        return  (ret);
    }

    *str = (char *)malloc(ret + 1); /* use process heap memory */
    if (*str == NULL) {
        sys_free(kstr); /* use sys_free() */
        return  (-1);
    }

    memcpy(*str, kstr, ret);

    sys_free(kstr); /* use sys_free() */

    return  (ret);
}

/*
 *  asprintf in process mode (use process memory)
 */
int  asprintf (char **str, char const *fmt, ...)
{
    va_list ap;
    int     ret;

    va_start(ap, fmt);
    ret = vasprintf(str, fmt, ap);
    va_end(ap);

    return  (ret);
}

