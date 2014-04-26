/*	$NetBSD: wctype.h,v 1.6 2005/02/03 04:39:32 perry Exp $	*/

/*-
 * Copyright (c)1999 Citrus Project,
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	citrus Id: wctype.h,v 1.4 2000/12/21 01:50:21 itojun Exp
 */

#ifndef _WCTYPE_H_
#define	_WCTYPE_H_

#ifndef _WCHAR_ALL_TYPE_
#define _WCHAR_ALL_TYPE_
typedef struct { int  dummy; }  mbstate_t;

/* 
 * the following code is also in stddef.h
 */
#ifndef _WINT_T
#define _WINT_T
#ifndef __WINT_TYPE__
#define __WINT_TYPE__ unsigned int
#endif
typedef __WINT_TYPE__ wint_t;
#endif /* _WINT_T */

typedef void *wctrans_t;
typedef enum {
    WC_TYPE_INVALID = 0,
    WC_TYPE_ALNUM,
    WC_TYPE_ALPHA,
    WC_TYPE_BLANK,
    WC_TYPE_CNTRL,
    WC_TYPE_DIGIT,
    WC_TYPE_GRAPH,
    WC_TYPE_LOWER,
    WC_TYPE_PRINT,
    WC_TYPE_PUNCT,
    WC_TYPE_SPACE,
    WC_TYPE_UPPER,
    WC_TYPE_XDIGIT,
    WC_TYPE_MAX
} wctype_t;
#endif

#ifdef	_BSD_WINT_T_
typedef	_BSD_WINT_T_    wint_t;
#undef	_BSD_WINT_T_
#endif

#ifdef	_BSD_WCTRANS_T_
typedef	_BSD_WCTRANS_T_	wctrans_t;
#undef	_BSD_WCTRANS_T_
#endif

#ifdef	_BSD_WCTYPE_T_
typedef	_BSD_WCTYPE_T_	wctype_t;
#undef	_BSD_WCTYPE_T_
#endif

#ifndef WEOF
#define	WEOF	((wint_t)-1)
#endif

__BEGIN_DECLS
int	iswalnum(wint_t);
int	iswalpha(wint_t);
int	iswblank(wint_t);
int	iswcntrl(wint_t);
int	iswdigit(wint_t);
int	iswgraph(wint_t);
int	iswlower(wint_t);
int	iswprint(wint_t);
int	iswpunct(wint_t);
int	iswspace(wint_t);
int	iswupper(wint_t);
int	iswxdigit(wint_t);
int	iswctype(wint_t, wctype_t);
wint_t	towctrans(wint_t, wctrans_t);
wint_t	towlower(wint_t);
wint_t	towupper(wint_t);
wctrans_t wctrans(const char *);
wctype_t wctype(const char *);
__END_DECLS

#endif		/* _WCTYPE_H_ */
