/*
** dbmisc.h
*/

#ifndef SUPPORT_H
#define SUPPORT_H

#define MAX_PATCH_ID     999999
#define MAX_ENDPOINT_ID  999999

#define EP_TYPE_OUTLET     1
#define EP_TYPE_SPLITTER   2
#define EP_TYPE_EQUIPMENT  3

typedef struct
{
    int id;
    unsigned int type;
    char *name;
    char *port;
    char *class;
    char *room;
    char *comment;
} ENDPOINT;

typedef struct
{
    int id;
    int p1, p2;
    char *comment;
    char *date;
} PATCH;


extern MYSQL *mysql;

extern char *
xstrdup(const char *str);

extern int
xprintf(char *buf,
	unsigned int bufsize,
	const char *format,
	...);

extern void
fatal(const char *msg, ...);

extern char *
fix_port(const char *name);

extern char *
fix_room(const char *name);




extern void
db_close(void);

extern int
db_init(const char *user,
	const char *pass,
	const char *name,
	const char *host);

extern const char *
db_error(void);


extern int
sql(const char *query);

extern char *
str2sql(const char *str,
	int force_quote);


extern void
endpoint_init(ENDPOINT *ep);

extern void
endpoint_reset(ENDPOINT *ep);

extern int
endpoint_lookup(ENDPOINT *ep,
		const char *name,
		const char *room);

extern int
endpoint_lookup_id(ENDPOINT *ep,
		   int eid);

extern int
endpoint_foreach(int (*fun)(ENDPOINT *pp, void *xp),
		 PLIST *idlist,
		 unsigned int type,
		 const char *name,
		 const char *room,
		 const char *class,
		 const char *comment,
		 void *xp,
		 const char *order);

extern int
endpoint_insert(ENDPOINT *ep);

extern int
endpoint_update_name(int id,
		     const char *nameport);

extern int
endpoint_update_type(int id,
		     unsigned int type);

extern int
endpoint_update_class(int id,
		      const char *class);

extern int
endpoint_update_room(int id,
		     const char *room);

extern int
endpoint_update_comment(int id,
			const char *comment);

extern int
endpoint_delete(int id);

extern void
patch_init(PATCH *pp);

extern void
patch_reset(PATCH *pp);

extern int
patch_lookup_endpoint(PATCH *pp,
		      int epid);

extern int
patch_lookup_id(PATCH *pp,
		int pid);

extern int
patch_foreach(int (*fun)(PATCH *pp, void *xp),
	      PLIST *idlist,
	      int type,
	      const char *name,
	      const char *room,
	      const char *class,
	      const char *comment,
	      void *xp,
	      const char *order);

extern int
valid_room(const char *room);

extern int
valid_port(const char *port);


extern int
patch_update_comment(int id,
		     const char *comment);

extern int
patch_update_endpoint(int pid,
		      int eno,
		      int eid);

extern int
patch_insert(PATCH *pp);

extern int
patch_remove(unsigned int id);

extern int
patch_find_highest(const char *room);

extern int
patch_delete(int pid);

extern const char *
db_info(void);

extern const char *
db_stat(void);

int
db_get_rows(const char *table,
	    const char *where);


extern char *
endpoint2str(ENDPOINT *ep,
	     char *buf,
	     int bufsize,
	     const char *inroom);

extern char *
endpoint_type2str(int t, int long_f);

extern int
endpoint_str2type(const char *s);

#endif
