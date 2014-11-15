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
#include <SylixOS.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <pwd.h>
#include <limits.h>

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

/*
 *  environ
 */
char **environ;

extern void __vp_patch_lock(void);
extern void __vp_patch_unlock(void);

#define __ENV_LOCK()      __vp_patch_lock()
#define __ENV_UNLOCK()    __vp_patch_unlock()

/*
 *  _init_env
 */
__attribute__((constructor)) static void _init_env (void)
{
    int  sysnum;
    int  dupnum;
    struct passwd   passwd;
    struct passwd  *ppasswd = LW_NULL;
    char user[LOGIN_NAME_MAX];
    
    sysnum = API_TShellVarGetNum();
    
    environ = malloc(sizeof(char *) * (sysnum + 1)); /* allocate environ space */
    if (!environ) {
        return;
    }
    
    dupnum = API_TShellVarDup(malloc, environ, sysnum);
    if (dupnum < 0) {
        dupnum = 0;
    }
    
    environ[dupnum] = NULL;

    if (!getpwuid_r(getuid(), &passwd, user, LOGIN_NAME_MAX, &ppasswd)) {
        setenv("USER", user, 1);
        setenv("HOME", ppasswd->pw_dir, 1);
    }
}

/*
 *  _deinit_env
 */
__attribute__((destructor)) static void _deinit_env (void)
{
    int  i;
    
    if (environ) {
        for (i = 0; environ[i]; i++) {
            free(environ[i]);
        }
        free(environ);
    }
}

/*
 *  vprocess entry point
 */
int _start (int argc, char **argv, char **env)
{
    int  i;
    int  oldnum = 0;
    int  addnum = 0;

    extern FUNCPTR vprocGetMain(VOID);
    
    FUNCPTR pfuncMain  = vprocGetMain(); /* must use MAIN MODULE main() function! */
    ULONG   ulVersion  = API_KernelVersion();
    ULONG   ulVerPatch = API_KernelVerpatch();
    
    /*
     *  check sylixos version
     */
    if ((ulVersion < __SYLIXOS_MAKEVER(1, 0, 0)) ||
        ((ulVersion == __SYLIXOS_MAKEVER(1, 0, 0)) && (ulVerPatch < 0x37))) {
        fprintf(stderr, "sylixos version MUST higher than 1.0.0-rc37\n");
        return  (EXIT_FAILURE);
    }
    
    /*
     * init command-line environ
     */
    if (env) {
        for (i = 0; env[i]; i++) {
            addnum++;
        }
        if (addnum > 0) {
            if (environ) {
                for (i = 0; environ[i]; i++) {
                    oldnum++;
                }
            }
            environ = realloc(environ, sizeof(char *) * (addnum + oldnum + 1));
            if (environ) {
                for (i = 0; i < addnum; i++) {
                    environ[oldnum + i] = strdup(env[i]);
                }
                environ[addnum + oldnum] = NULL;
            }
        }
    }

    errno = 0;
    return  (pfuncMain(argc, argv, environ));
}

/*
 *  the following code is from BSD lib
 */
/*
 *  __findenv
 */
static char *__findenv (const char *name, int *offset)
{
    size_t len;
    const char *np;
    char **p, *c;

    if (name == NULL || environ == NULL)
        return NULL;
    for (np = name; *np && *np != '='; ++np)
        continue;
    len = np - name;
    for (p = environ; (c = *p) != NULL; ++p)
        if (strncmp(c, name, len) == 0 && c[len] == '=') {
            *offset = p - environ;
            return c + len + 1;
        }
    return NULL;
}

/*
 *  getenv
 */
char *getenv (const char *name)
{
    int offset;
    char *result;

    if (!name) {
        return (NULL);
    }

    __ENV_LOCK();
    result = __findenv(name, &offset);
    __ENV_UNLOCK();
    return result;
}

/*
 *  getenv_r
 */
int getenv_r (const char *name, char *buf, int len)
{
    int offset;
    char *result;
    int rv = -1;

    if (!name || !buf) {
        return (-1);
    }

    __ENV_LOCK();
    result = __findenv(name, &offset);
    if (result == NULL) {
        errno = ENOENT;
        goto out;
    }
    if (strlcpy(buf, result, len) >= len) {
        errno = ERANGE;
        goto out;
    }
    rv = 0;
out:
    __ENV_UNLOCK();
    return rv;
}

/*
 *  setenv
 */
int setenv (const char *name, const char *value, int rewrite)
{
    static char **saveenv;    /* copy of previously allocated space */
    char *c, **newenv;
    const char *cc;
    size_t l_value, size;
    int offset;

    if (!name || !value) {
        return (-1);
    }

    if (*value == '=')            /* no `=' in value */
        ++value;
    l_value = strlen(value);
    __ENV_LOCK();
    /* find if already exists */
    if ((c = __findenv(name, &offset)) != NULL) {
        if (!rewrite)
            goto good;
        if (strlen(c) >= l_value)    /* old larger; copy over */
            goto copy;
    } else {                    /* create new slot */
        size_t cnt;

        for (cnt = 0; environ[cnt]; ++cnt)
            continue;
        size = (size_t)(sizeof(char *) * (cnt + 2));
        if (saveenv == environ) {        /* just increase size */
            if ((newenv = realloc(saveenv, size)) == NULL)
                goto bad;
            saveenv = newenv;
        } else {                /* get new space */
            free(saveenv);
            if ((saveenv = malloc(size)) == NULL)
                goto bad;
            (void)memcpy(saveenv, environ, cnt * sizeof(char *));
        }
        environ = saveenv;
        environ[cnt + 1] = NULL;
        offset = (int)cnt;
    }
    for (cc = name; *cc && *cc != '='; ++cc)    /* no `=' in name */
        continue;
    size = cc - name;
    /* name + `=' + value */
    if ((environ[offset] = malloc(size + l_value + 2)) == NULL)
        goto bad;
    c = environ[offset];
    (void)memcpy(c, name, size);
    c += size;
    *c++ = '=';
copy:
    (void)memcpy(c, value, l_value + 1);
good:
    __ENV_UNLOCK();
    return 0;
bad:
    __ENV_UNLOCK();
    return -1;
}

/*
 *  unsetenv
 */
int unsetenv (const char *name)
{
    char **p;
    int offset;

    if (name == NULL || *name == '\0' || strchr(name, '=') != NULL) {
        errno = EINVAL;
        return -1;
    }

    __ENV_LOCK();
    while (__findenv(name, &offset))    /* if set multiple times */
        for (p = &environ[offset];; ++p)
            if (!(*p = *(p + 1)))
                break;
    __ENV_UNLOCK();

    return 0;
}
