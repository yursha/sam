/* Copyright (c) 1998 Lucent Technologies - All rights reserved. */

    /* Plan 9 C library interface */


#include <u.h>

#define utflen(s)        (mbstowcs(NULL, (s), 0))
#define fullrune(s, n)   (mbtowc(NULL, (s), (n))) /* >0 */
#define runetochar(s, r) (wctomb((s), (r)))
#define runelen(r)       (wctomb(NULL, (r)))

#define sprint              sprintf
#define dup(a,b)            dup2(a,b)
#define seek(a,b,c)         lseek(a,b,c)
#define create(name, mode, perm)    creat(name, perm)
#define exec(a,b)           execv(a,b)

#define _exits(v)           _exit((v) ? 1 : 0)

/*
 * new rune routines
 */
extern  int chartorune(wchar_t*, char*);

/*
 *  Miscellaneous functions
 */
extern  int notify (void(*)(void *, char *));
extern  char*   getuser(void);
extern  void    exits(char*);

