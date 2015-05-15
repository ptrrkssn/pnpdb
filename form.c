#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "form.h"

static int nvar = 0;
static FORMVAR fvar[MAXVARS];

static char form_row[64] = "";

void
form_set_row(unsigned int row)
{
    if (row)
	sprintf(form_row, "%u", row);
    else
	strcpy(form_row, "");
}

int
hextochr(char *str)
{
    return ((isdigit((unsigned char) (str[0])) ?
	     str[0]-'0' : toupper(str[0])-'A'+10) << 4) +
	(isdigit((unsigned char) (str[1])) ?
	 str[1]-'0' : toupper(str[1])-'A'+10);
}


void
http_print(FILE *fp, const char *str)
{
  int c;

  while ((c = (unsigned char) *str++) != '\0')
    {
      if (c == ' ')
	putc('+', fp);
      else if (c < ' ' || !isascii(c) || c == '=' || c == '&' || c == '?')
	fprintf(fp, "%%%02X", c);
      else
	putc(c, fp);
    }
}


char *
http_strip(char *str)
{
    char *dst, *src;

    dst = src = str;
    while (*src)
    {
	if (*src == '+')
	{
	    *dst++ = ' ';
	    ++src;
	    continue;
	}

	if (*src == '%')
	{
	    if (src[1] == '%')
	    {
		*dst++ = '%';
		src += 2;
		continue;
	    }

	    if (isxdigit((unsigned char) (src[1])) &&
		isxdigit((unsigned char) (src[2])))
	    {
		*dst++ = hextochr(src+1);
		src += 3;
		continue;
	    }
	}
	    
	*dst++ = *src++;
    }

    *dst = '\0';

    return str;
}


void
form_add(const char *var,
	 const char *val)
{
    int i;

    for (i = 0; i < nvar && strcmp(var, fvar[i].name) != 0; i++)
	;

    if (i < nvar && fvar[i].value)
    {
	char *buf = malloc(strlen(fvar[i].value)+2+strlen(val));
	strcpy(buf, fvar[i].value);
	strcat(buf, " ");
	strcat(buf, val);
	free(fvar[i].value);
	fvar[i].value = buf;
    }
    else 
    {
	fvar[i].value = (val ? strdup(val) : NULL);
	
	if (i >= nvar)
	{
	    fvar[i].name = strdup(var);
	    ++nvar;
	}
    }
}


int
form_init(FILE *fp)
{
    char var[256];
    char val[10000];
    char *cp, *vp;
    int c;
    
    char *ep = getenv("QUERY_STRING");
    char *rm = getenv("REQUEST_METHOD");

    nvar = 0;

    if (ep && *ep && (rm && strcmp(rm, "POST") != 0))
    {
	cp = strtok(ep, "&");
	while (cp)
	{
	    vp = strchr(cp, '=');
	    if (vp)
	    {
		*vp++ = '\0';
		http_strip(vp);
	    }
	    
	    http_strip(cp);
	    form_add(cp, vp);
	    
	    cp = strtok(NULL, "&");
	}
    }
    else if (fp != NULL)
    {
	while (fscanf(fp, "%255[^=]=", var) == 1)
	{
	    val[0] = '\0';
	    
	    http_strip(var);
	    
	    c = getc(fp);
	    if (c == EOF)
	    {
		form_add(var, "");
		break;
	    }
	    
	    ungetc(c, fp);
	    if (c != '&' && fscanf(fp, "%9999[^&\n\r]", val) == 1)
	    {
		http_strip(val);
		form_add(var, val);
	    }
	    else
		form_add(var, "");
	    
	    c = getc(fp);
	    if (c != '&')
		ungetc(c, fp);
	}
    }
    
    return nvar;
}



const char *
form_getvar(const char *name)
{
    int i;
    char buf[256];

    if (*form_row)
    {
	strcpy(buf, name);
	strcat(buf, form_row);
	name = buf;
    }
    
    for (i = 0; i < nvar && strcmp(name, fvar[i].name) != 0; i++)
	;

    if (i >= nvar)
	return NULL;

    return fvar[i].value;
}


int
form_gets(const char *name,
	  char **var)
{
    const char *str = form_getvar(name);

    if (!str)
	return -1;

    *var = strdup(str);
    return (str[0] ? 1 : 0);
}


char *
form_get(const char *name)
{
    const char *val = form_getvar(name);
    if (!val)
	return NULL;

    return strdup(val);
}


int
form_getv(const char *name,
	  const char *format,
	  void *var)
{
    const char *str = form_getvar(name);

    if (!str)
	return -1;
    return sscanf(str, format, var);
}

int
form_geti(const char *name,
	  int *var)
{
    return form_getv(name, "%d", var);
}

int
form_getu(const char *name,
	  unsigned int *var)
{
    return form_getv(name, "%u", var);
}


int
form_foreach(int (*fun)(const char *var, const char *val, void *x), void *x)
{
  int i, rv;


  rv = 0;
  for (i = 0; i < nvar && rv == 0; i++)
    rv = fun(fvar[i].name, fvar[i].value, x);

  return rv;
}

void
form_cgi_post(FILE *fp)
{
  int i;

  for (i = 0; i < nvar; i++)
    {
      http_print(fp, fvar[i].name);
      putc('=', fp);
      http_print(fp, fvar[i].value);
    }
}

char *
strtrim(const char *buf)
{
    char *cp, *res;


    if (!buf)
	return NULL;
    
    res = strdup(buf);
    if (!res)
	return NULL;
    
    for (cp = res; *cp && isspace(*cp); ++cp)
	;

    if (cp > res)
	strcpy(res, cp);
    
    for (cp = res; *cp; ++cp)
	;

    while (--cp >= res && isspace(*cp))
	;

    cp[1] = '\0';

    return res;
}

