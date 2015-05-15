/*
** plist.c
*/

#include <stdlib.h>
#include <string.h>

#include "plist.h"


PLIST *
plist_init(int size)
{
    PLIST *lp = NULL;

    
    lp = malloc(sizeof(*lp));
    if (!lp)
	return NULL;

    if (!size)
	size = PLIST_DEFAULT_SIZE;
    
    lp->len = 0;
    lp->v = malloc(sizeof(void *)*size);
    if (!lp->v)
    {
	free(lp);
	return NULL;
    }

    lp->size = size;
    return lp;
}


void
plist_free(PLIST *lp)
{
    free(lp->v);
    free(lp);
}


int
plist_append(PLIST *lp,
	    const char *sp)
{
    if (!sp)
	return 0;
    
    if (lp->len == lp->size)
    {
	int nsize;
	char **tv;

	nsize = lp->size+PLIST_DEFAULT_SIZE;
	tv = realloc(lp->v, sizeof(char *)*nsize);
	if (!tv)
	    return -1;

	lp->v = tv;
	lp->size = nsize;
    }

    lp->v[lp->len++] = strdup(sp);
    return lp->len;
}
    

int
plist_add(PLIST *lp,
	 const char *sp)
{
    int i;


    if (!sp)
	return 0;
    
    for (i = 0; i < lp->len && strcmp(lp->v[i], sp); i++)
	;

    if (i < lp->len)
    {
	free(lp->v[i]);
	lp->v[i] = strdup(sp);
	return lp->len;
    }
    else
	return plist_append(lp, sp);
}

static int (*sort_fun)(const char *s1, const char *s2) = NULL;

static int sort_helper(const void *p1,
		       const void *p2)
{
    const char *s1 = *(const char **) p1;
    const char *s2 = *(const char **) p2;

    return (*sort_fun)(s1, s2);
    
}
void
plist_sort(PLIST *lp,
	  int (*cfun)(const char *s1, const char *s2))
{
    if (cfun)
	sort_fun = cfun;
    else
	sort_fun = strcmp;
    
    qsort(&lp->v[0], sizeof(lp->v[0]),
	  lp->len, sort_helper);
}


PLIST *
plist_parse(PLIST *lp,
	   const char *sp,
	   const char *dp)
{
    char *ts, *cp, *lastp;


    if (!sp)
	return NULL;

    ts = strdup(sp);
    if (!ts)
	return NULL;

    if (!lp)
    {
	lp = plist_init(0);
	if (!lp)
	{
	    free(ts);
	    return NULL;
	}
    }
    
    cp = strtok_r(ts, dp, &lastp);
    while (cp)
    {
	if (plist_add(lp, cp) < 0)
	    return lp;
	
	cp = strtok_r(NULL, dp, &lastp);
    }

    return lp;
}


#undef plist_length
int
plist_length(PLIST *lp)
{
    if (!lp)
	return -1;

    return lp->len;
}

#undef plist_get
char *
plist_get(PLIST *lp,
	  unsigned int idx)
{
    if (!lp)
	return NULL;

    if (idx < 0 || idx >= lp->len)
	return NULL;
    
    return lp->v[idx];
}

char *
plist_export(PLIST *lp,
	     const char *sep,
	     char *(*qfun)(const char *str))
{
    int i, len, seplen, tlen;
    int bufsize = 16384;
    char *buf = NULL, *cp;


    if (!lp)
	return "";
    
    if (!sep)
	sep=",";
    
    seplen = strlen(sep);

    buf = malloc(bufsize);
    if (!buf)
	return NULL;

    bufsize--;
    
    len = 0;
    for (i = 0; i < lp->len && len < bufsize; i++)
    {
	if (i != 0)
	{
	    if (len+seplen >= bufsize)
		return NULL;
	    
	    strcpy(buf+len, sep);
	    len += seplen;
	}

	if (qfun)
	{
	    cp = qfun(lp->v[i]);
	    tlen = strlen(cp);
	    if (len+tlen >= bufsize)
		return NULL;
	    
	    strcpy(buf+len, cp);
	    free(cp);
	    len += tlen;
	}
	else
	{
	    tlen = strlen(lp->v[i]);
	    if (len+tlen >= bufsize)
		return NULL;
	    
	    strcpy(buf+len, lp->v[i]);
	    len += tlen;
	}
    }

    return buf;
}
