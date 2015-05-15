/*
** plist.h
*/

#ifndef PLIST_H
#define PLIST_H 1

#define PLIST_DEFAULT_SIZE 32

typedef struct
{
    int len;
    int size;
    char **v;
} PLIST;

extern PLIST *
plist_init(int size);

extern void
plist_free(PLIST *lp);

extern int
plist_append(PLIST *lp,
	     const char *sp);

extern int
plist_add(PLIST *lp,
	 const char *sp);

extern void
plist_sort(PLIST *lp,
	  int (*cfun)(const char *s1, const char *s2));

extern PLIST *
plist_parse(PLIST *lp,
	   const char *sp,
	   const char *dp);


extern int
plist_length(PLIST *lp);

extern char *
plist_get(PLIST *lp,
	  unsigned int idx);

#define plist_length(L) ((L)->len)
#define plist_get(L,I)  ((L)->v[(I)])

extern char *
plist_export(PLIST *lp,
	     const char *sep,
	     char *(*qfun)(const char *str));

#endif
