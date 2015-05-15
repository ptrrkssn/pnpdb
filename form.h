#ifndef PTMS_FORM_H
#define PTMS_FORM_H

typedef struct
{
    char *name;
    char *value;
} FORMVAR;

#define MAXVARS 1024

extern int
form_init(FILE *fp);


extern const char *
form_getvar(const char *name);


extern int
form_gets(const char *name,
	  char **var);

extern int
form_getv(const char *name,
	  const char *format,
	  void *var);

extern int
form_getu(const char *name,
	  unsigned int *var);

extern int
form_geti(const char *name,
	  int *var);


extern char *
form_get(const char *name);


extern void
form_cgi_post(FILE *fp);

extern int
form_foreach(int (*fun)(const char *var, const char *val, void *x), void *x);


extern void
form_set_row(unsigned int row);

#endif
