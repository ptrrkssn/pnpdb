#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include <mysql.h>

#include "html.h"
#include "plist.h"
#include "dbmisc.h"


extern int debug;
extern int nowrite;

MYSQL *mysql = NULL;



int
xprintf(char *buf,
	unsigned int bufsize,
	const char *format,
	...)
{
    va_list ap;
    int len, rc = -1;


    va_start(ap, format);
    
    len = strlen(buf);
    if (len < bufsize)
    {
	rc = vsnprintf(buf+len, bufsize-len, format, ap);
	va_end(ap);
    }

    if (rc < 0 || strlen(buf+len) != rc)
	return -1;

    return strlen(buf);
}



char *
xstrdup(const char *str)
{
    if (!str)
	return NULL;

    return strdup(str);
}

void
fatal(const char *msg, ...)
{
    va_list ap;


    va_start(ap, msg);

    if (getenv("REQUEST_URI")) {
      h_header("Error");
      fflush(stdout);
      printf("ERROR: ");
      vprintf(msg, ap);
      putchar('\n');
      fflush(stdout);
      h_footer(NULL);
    } else {
      fprintf(stderr, "Error: ");
      vprintf(msg, ap);
      putc('\n', stderr);
    }

    va_end(ap);
    exit(1);
}

int
db_init(const char *user,
	const char *pass,
	const char *name,
	const char *host)
{
    mysql = mysql_init(NULL);
    if (!mysql)
	return -1;

    if (!mysql_real_connect(mysql, host ? host : "mysql", 
			    user, pass ? pass : "", name, 0, NULL, 0))
	return -2;

    return 0;
}

void
db_close(void)
{
  mysql_close(mysql);
}

const char *
db_error(void)
{
    const char *cp;
    
    if (mysql)
    {
	cp = mysql_error(mysql);
	if (cp)
	    return cp;
	else
	    return "No SQL error";
    }
    else
	return "Unknown error";
}

const char *
db_info(void) {
  const char *s;
  s = mysql_info(mysql);
  if (!s)
    return "";

  return s;
}

const char *
db_stat(void) {
  const char *s;
  s = mysql_stat(mysql);
  if (!s)
    return "";

  return s;
}

int
sql(const char *query)
{
    int rc;
    int rows;

    
    rc = mysql_real_query(mysql,query,strlen(query));

    if (strncmp(query, "UPDATE", 6) == 0 ||
	strncmp(query, "DELETE", 6) == 0 ||
	strncmp(query, "INSERT", 6) == 0)
    {
	rows = mysql_affected_rows(mysql);
    }
    else
	rows = 0;
    
    if (debug)
	printf("<!-- SQL: %s -> %d (%s) / %d rows -->\n", query, rc, rc ? mysql_error(mysql) : "OK", rows);

    if (rc > 0)
	return -rc;
    else if (rc)
	return rc;

    return rows;
}


char *
str2sql(const char *str,
	int mode)
{
    int len;
    char *buf;
    const char *cp;
    int quote = mode;
    
    
    if (!str || (!quote && !*str))
	return "NULL";

    if (!*str)
	return "\"\"";
    
    len = strlen(str);
    buf = malloc(len*2+3);
    if (!buf)
	return NULL;

    for (cp = str; isdigit(*cp); ++cp)
	;

    if (quote || *cp)
    {
	buf[0] = '"';
	len = mysql_real_escape_string(mysql, buf+1, str, len);
	buf[len+1] = '"';
	buf[len+2] = '\0';
    }
    else
	len = mysql_real_escape_string(mysql, buf, str, len);
    
    return buf;
}


char *
str2sql_q(const char *str)
{
    return str2sql(str, 1);
}

char *
str2sql_a(const char *str)
{
    return str2sql(str, 0);
}



void
endpoint_init(ENDPOINT *ep)
{
    memset(ep, 0, sizeof(*ep));
}

void
endpoint_reset(ENDPOINT *ep)
{
    if (ep->name)
	free(ep->name);
    if (ep->port)
	free(ep->port);
    if (ep->room)
	free(ep->room);
    if (ep->class)
	free(ep->class);
    if (ep->comment)
	free(ep->comment);

    endpoint_init(ep);
}


int
endpoint_lookup_id(ENDPOINT *ep,
		   int eid)
{
    char buf[2048];
    MYSQL_RES *res = NULL;
    MYSQL_ROW row;
    int n, fields, id;
    

    buf[0] = 0;
    if (xprintf(buf, sizeof(buf), "SELECT id,type,name,port,class,room,comment FROM endpoints WHERE id=%d", eid) < 0)
	return -7;
		
    if (sql(buf) < 0)
	return -1;
    
    res = mysql_store_result(mysql);
    if (!res)
	return -2;
    
    fields = mysql_num_fields(res);
    if (fields != 7)
    {
	mysql_free_result(res);
	return -3;
    }

    id = -1;
    n = 0;
    while ((row = mysql_fetch_row(res)))
    {
	++n;

	if (n > 1)
	{
	    mysql_free_result(res);
	    return -4;
	}

	endpoint_init(ep);
	
	if (!row[0] || sscanf(row[0], "%u", &ep->id) != 1)
	{
	    mysql_free_result(res);
	    return -5;
	}

	id = ep->id;
	
	if (!row[1] || sscanf(row[1], "%u", &ep->type) != 1)
	{
	    mysql_free_result(res);
	    return -6;
	}

	ep->name = xstrdup(row[2]);
	ep->port = xstrdup(row[3]);
	ep->class = xstrdup(row[4]);
	ep->room = xstrdup(row[5]);
	ep->comment = xstrdup(row[6]);
    }

    mysql_free_result(res);
    
    if (n == 1)
	return id;

    return 0;
}




int
endpoint_lookup(ENDPOINT *ep,
		const char *name,
		const char *inroom)
{
    ENDPOINT ebuf;
    char buf[2048], *tname, *port, *cp, c;
    MYSQL_RES *res = NULL;
    MYSQL_ROW row;
    int n, fields, id, rc;
    

    if (!ep)
    {
	ep = &ebuf;
	endpoint_init(ep);
    }

    if (name[0] != '0' && sscanf(name, "%u%c", &id, &c) == 1)
	return endpoint_lookup_id(ep, id);
    
    tname = xstrdup(name);
    cp = strchr(tname, '@');
    if (cp)
    {
	*cp++ = '\0';
	inroom = cp;
    }

    port = strchr(tname, ':');
    if (port)
	*port++ = '\0';
	
    buf[0] = 0;
    rc = xprintf(buf, sizeof(buf), "SELECT id,type,name,port,class,room,comment FROM endpoints WHERE name=%s",
		 str2sql_q(tname));

    if (port)
	rc = xprintf(buf, sizeof(buf), " AND port=%s", str2sql_q(port));
		
    if (inroom)
	rc = xprintf(buf, sizeof(buf), " AND room=%s", str2sql_q(inroom));

    if (rc < 0)
	return -7;
    
    if (sql(buf) < 0)
	return -1;
    
    res = mysql_store_result(mysql);
    if (!res)
	return -2;
    
    fields = mysql_num_fields(res);
    if (fields != 7)
    {
	mysql_free_result(res);
	return -3;
    }

    id = -1;
    n = 0;
    while ((row = mysql_fetch_row(res)))
    {
	++n;

	if (n > 1)
	{
	    mysql_free_result(res);
	    return -4;
	}

	endpoint_init(ep);
	
	if (!row[0] || sscanf(row[0], "%u", &ep->id) != 1)
	{
	    mysql_free_result(res);
	    return -5;
	}

	id = ep->id;
	
	if (!row[1] || sscanf(row[1], "%u", &ep->type) != 1)
	{
	    mysql_free_result(res);
	    return -6;
	}

	ep->name = xstrdup(row[2]);
	ep->port = xstrdup(row[3]);
	ep->class = xstrdup(row[4]);
	ep->room = xstrdup(row[5]);
	ep->comment = xstrdup(row[6]);
    }

    mysql_free_result(res);
    
    if (n == 1)
    {
	if (ep == &ebuf)
	    endpoint_reset(ep);
	
	return id;
    }

    return 0;
}

int
endpoint_insert(ENDPOINT *ep)
{
    char buf[2048];

    
    if (!ep || ep->type < 1)
	return -1;

    buf[0] = 0;
    xprintf(buf, sizeof(buf), "INSERT INTO endpoints VALUE(");
    
    if (ep->id)
	xprintf(buf, sizeof(buf), "%d", ep->id);
    else
	xprintf(buf, sizeof(buf), "NULL");
	
    if (xprintf(buf, sizeof(buf), ",%d,%s,%s,%s,%s,%s)",
		ep->type,
		str2sql_q(ep->name),
		str2sql_q(ep->port && ep->port[0] ? ep->port : ""),
		str2sql_q(ep->class),
		str2sql_q(ep->room),
		str2sql_q(ep->comment)) < 0)
	return -1;

    return sql(buf);
}


int
endpoint_delete(int id)
{
    char buf[2048];

    
    if (id < 1 || id > MAX_ENDPOINT_ID)
	return -1;

    /* Endpoints can only be deleted if there are no patches connected to them */
    buf[0] = 0;
    if (xprintf(buf, sizeof(buf), "DELETE FROM endpoints WHERE id=%d AND NOT EXISTS (SELECT id FROM patches WHERE endpoint1=%d OR endpoint2=%d)", id, id, id) < 0)
	return -1;
    
    return sql(buf);
}

int
endpoint_update_name(int id,
		     const char *nameport)
{
    char buf[2048];
    char *name = strdup(nameport);
    char *port = strchr(name, ':');
    if (port)
	*port++ = '\0';
    
    if (id < 1 || id > MAX_ENDPOINT_ID)
	return -1;

    buf[0] = 0;
    
    if (port)
    {
	if (xprintf(buf, sizeof(buf), "UPDATE endpoints SET name=%s, port=%s WHERE id=%d",
		    str2sql_q(name),
		    str2sql_q(port), id) < 0)
	    return -1;
    }
    else
    {
	if (xprintf(buf, sizeof(buf), "UPDATE endpoints SET name=%s WHERE id=%d",
		    str2sql_q(name), id) < 0)
	    return -1;
    }
    
    return sql(buf);
    
}


int
endpoint_update_class(int id,
		     const char *class)
{
    char buf[2048];

    
    if (id < 1 || id > MAX_ENDPOINT_ID)
	return -1;

    buf[0] = 0;
    if (xprintf(buf, sizeof(buf), "UPDATE endpoints SET class=%s WHERE id=%d",
		str2sql_q(class), id) < 0)
	return -1;
    
    return sql(buf);
    
}


int
endpoint_update_type(int id,
		     unsigned int type)
{
    char buf[2048];

    
    if (id < 1 || id > MAX_ENDPOINT_ID)
	return -1;

    buf[0] = 0;
    if (xprintf(buf, sizeof(buf), "UPDATE endpoints SET type=%u WHERE id=%d",
		type, id) < 0)
	return -1;
    
    return sql(buf);
    
}


int
endpoint_update_room(int id,
		     const char *room)
{
    char buf[2048];

    
    if (id < 1 || id > MAX_ENDPOINT_ID)
	return -1;

    buf[0] = 0;
    if (xprintf(buf, sizeof(buf), "UPDATE endpoints SET room=%s WHERE id=%d",
		str2sql_q(room), id) < 0)
	return -1;
    
    return sql(buf);
    
}


int
endpoint_update_comment(int id,
			const char *comment)
{
    char buf[2048];

    
    if (id < 1 || id > MAX_ENDPOINT_ID)
	return -1;

    buf[0] = 0;
    if (xprintf(buf, sizeof(buf), "UPDATE endpoints SET comment=%s WHERE id=%d",
		str2sql_q(comment), id) < 0)
	return -1;
    
    return sql(buf);
    
}

void
patch_init(PATCH *pp)
{
    memset(pp, 0, sizeof(*pp));
}

void
patch_reset(PATCH *pp)
{
    if (pp->comment)
	free(pp->comment);
    if (pp->date)
	free(pp->date);
    patch_init(pp);
}


int
patch_remove(unsigned int id)
{
    char buf[2048];

    
    if (id < 1 || id > MAX_PATCH_ID)
	return -1;
    
    buf[0] = 0;
    if (xprintf(buf, sizeof(buf), "DELETE FROM patches WHERE id=%u", id) < 0)
	return -1;
    
    return sql(buf);
}

int
patch_insert(PATCH *pp)
{
    char buf[2048];
    PATCH pbuf;
    int rc;
    
    
    if (!pp || pp->id < 1 || pp->id > MAX_PATCH_ID || pp->p1 == pp->p2)
	return -1;

    if (pp->p1 > 0 && (rc = patch_lookup_endpoint(&pbuf, pp->p1)))
    {
	printf("<!-- patch_lookup_id(%d) -> %d -->\n", pp->p1, rc);
	return -2;
    }
    
    if (pp->p2 > 0 && (rc = patch_lookup_endpoint(&pbuf, pp->p2)))
    {
	printf("<!-- patch_lookup_id(%d) -> %d -->\n", pp->p2, rc);
	return -3;
    }
    
    buf[0] = 0;
    if (xprintf(buf, sizeof(buf), "INSERT INTO patches VALUES(%d,%d,%d,%s,NULL)",
		pp->id,
		pp->p1,
		pp->p2,
		str2sql_q(pp->comment)) < 0)
	return -1;

    return sql(buf);
}

int
patch_delete(int pid)
{
    char buf[2048];

    
    if (pid < 1 || pid > MAX_PATCH_ID)
	return -1;

    buf[0] = 0;
    if (xprintf(buf, sizeof(buf), "DELETE FROM patches WHERE id=%d", pid) < 0)
	return -1;
    
    return sql(buf);
}

int
patch_update_comment(int id,
		     const char *comment)
{
    char buf[2048];

    
    if (id < 1 || id > MAX_PATCH_ID)
	return -1;

    buf[0] = 0;
    if (xprintf(buf, sizeof(buf), "UPDATE patches SET comment=%s WHERE id=%d",
		str2sql_q(comment), id) < 0)
	return -1;
    
    return sql(buf);
    
}


int
patch_update_endpoint(int id,
		      int eno,
		      int eid)
{
    char buf[2048];

    if (id < 1 || id > 100000)
	return -1;

    buf[0] = 0;
    
    if (xprintf(buf, sizeof(buf), "UPDATE patches SET endpoint%d=%d WHERE id=%d", eno, eid, id) < 0)
	return -1;
    
    return sql(buf);
}


int
patch_lookup_endpoint(PATCH *pp,
		      int epid)
{
    PATCH pbuf;
    char buf[2048];
    MYSQL_RES *res = NULL;
    MYSQL_ROW row;
    int n, fields, id;
    

    id = -1;
    if (!pp)
    {
	pp = &pbuf;
	patch_init(pp);
    }

    buf[0] = 0;
    if (xprintf(buf, sizeof(buf), "SELECT id,endpoint1,endpoint2,comment,date FROM patches WHERE endpoint1=%d OR endpoint2=%d",
		epid, epid) < 0)
	return -1;

    if (sql(buf) < 0)
	return -1;
    
    res = mysql_store_result(mysql);
    if (!res)
	return -2;
    
    fields = mysql_num_fields(res);
    if (fields != 5)
    {
	mysql_free_result(res);
	return -3;
    }

    n = 0;
    while ((row = mysql_fetch_row(res)))
    {
	++n;

	if (n > 1)
	{
	    mysql_free_result(res);
	    return -4;
	}

	patch_init(pp);
	       
	if (!row[0] || sscanf(row[0], "%u", &pp->id) != 1)
	{
	    mysql_free_result(res);
	    return -5;
	}

	id = pp->id;
	
	if (!row[1] || sscanf(row[1], "%u", &pp->p1) != 1)
	{
	    mysql_free_result(res);
	    return -6;
	}

	if (!row[2] || sscanf(row[2], "%u", &pp->p2) != 1)
	{
	    mysql_free_result(res);
	    return -7;
	}

	pp->comment = xstrdup(row[3]);
	pp->date = xstrdup(row[4]);
    }

    mysql_free_result(res);
    
    if (n == 1)
    {
	if (pp == &pbuf)
	    patch_reset(pp);
	
	return id;
    }

    return 0;
}


int
patch_lookup_id(PATCH *pp,
		int pid)
{
    PATCH pbuf;
    char buf[2048];
    MYSQL_RES *res = NULL;
    MYSQL_ROW row;
    int n, fields, id;
    

    id = -1;
    if (!pp)
    {
	pp = &pbuf;
	patch_init(pp);
    }

    buf[0] = 0;
    if (xprintf(buf, sizeof(buf), "SELECT id,endpoint1,endpoint2,comment,date FROM patches WHERE id=%d", pid) < 0)
	return -1;
		
    if (sql(buf) < 0)
	return -1;
    
    res = mysql_store_result(mysql);
    if (!res)
	return -2;
    
    fields = mysql_num_fields(res);
    if (fields != 5)
    {
	mysql_free_result(res);
	return -3;
    }

    n = 0;
    while ((row = mysql_fetch_row(res)))
    {
	++n;

	if (n > 1)
	{
	    mysql_free_result(res);
	    return -4;
	}

	patch_init(pp);
	       
	if (!row[0] || sscanf(row[0], "%u", &pp->id) != 1)
	{
	    mysql_free_result(res);
	    return -5;
	}

	id = pp->id;
	
	if (!row[1] || sscanf(row[1], "%u", &pp->p1) != 1)
	{
	    mysql_free_result(res);
	    return -6;
	}

	if (!row[2] || sscanf(row[2], "%u", &pp->p2) != 1)
	{
	    mysql_free_result(res);
	    return -7;
	}

	pp->comment = xstrdup(row[3]);
	pp->date = xstrdup(row[4]);
    }

    mysql_free_result(res);
    
    if (n == 1)
    {
	if (pp == &pbuf)
	    patch_reset(pp);
	
	return id;
    }

    return 0;
}


int
patch_find_highest(const char *room)
{
    char buf[2048];
    MYSQL_RES *res = NULL;
    MYSQL_ROW row;
    int n, fields, id, rc;
    

    buf[0] = 0;
    if (room)
    {

	room = str2sql_q(room);
	rc = xprintf(buf, sizeof(buf),
		     "SELECT MAX(id) FROM patches WHERE endpoint1 IN (SELECT id FROM endpoints WHERE room=%s) OR endpoint2 IN (SELECT id FROM endpoints WHERE room=%s)",
		     room, room);
    }
    else
	rc = xprintf(buf, sizeof(buf), "SELECT MAX(id) FROM patches");

    if (rc < 0)
	return -1;
    
    if (sql(buf) < 0)
	return -1;
    
    res = mysql_store_result(mysql);
    if (!res)
	return -2;
    
    fields = mysql_num_fields(res);
    if (fields != 1)
    {
	mysql_free_result(res);
	return -3;
    }

    id = -1;
    n = 0;
    while ((row = mysql_fetch_row(res)))
    {
	++n;

	if (n > 1)
	{
	    mysql_free_result(res);
	    return -4;
	}

	if (!row[0] || sscanf(row[0], "%u", &id) != 1)
	{
	    mysql_free_result(res);
	    return -5;
	}
    }

    mysql_free_result(res);
    
    if (n == 1)
	return id;

    return 0;
}

	
	
int
endpoint_foreach(int (*fun)(ENDPOINT *pp, void *xp),
		 PLIST *patchlist,
		 unsigned int type,
		 const char *name,
		 const char *room,
		 const char *class,
		 const char *comment,
		 void *xp,
		 const char *order)
{
    ENDPOINT ebuf, *ep;
    char buf[2048], *tname, *cp;
    char *port = NULL;
    MYSQL_RES *res = NULL;
    MYSQL_ROW row;
    int fields, rc;
    int first_id, last_id;
    char dummy;

    
    if (name)
    {
	tname = xstrdup(name);
	cp = strchr(tname, '@');
	if (cp)
	{
	    *cp++ = '\0';
	    room = cp;
	}
	name = tname;

	port = strchr(tname, ':');
	if (port)
	    *port++ = '\0';

	name = tname;
    }

    buf[0] = 0;
    rc = xprintf(buf, sizeof(buf), "SELECT id,type,name,port,class,room,comment FROM endpoints");
	
    if (patchlist || type || name || room || class || comment)
    {
	rc = xprintf(buf, sizeof(buf), " WHERE TRUE");

	if (patchlist)
	{
	    rc = xprintf(buf, sizeof(buf), " AND ( id=(SELECT endpoint1 FROM patches WHERE id IN (%s))", plist_export(patchlist,",",str2sql_a));
	    rc = xprintf(buf, sizeof(buf), " OR id=(SELECT endpoint2 FROM patches WHERE id IN (%s)) )", plist_export(patchlist,",",str2sql_a));
	}
	    
	if (type)
	    rc = xprintf(buf, sizeof(buf), " AND type=%d", type);

	if (name)
	{
	  if ((sscanf(name, "%u-%u%c", &first_id, &last_id, &dummy) == 2 && 
	       first_id < MAX_ENDPOINT_ID && last_id < MAX_ENDPOINT_ID) || 
	      sscanf(name, "#%u-%u%c", &first_id, &last_id, &dummy) == 2) {
	    rc = xprintf(buf, sizeof(buf), " AND id >= %u AND id <= %u", first_id, last_id);
	  } else if ((sscanf(name, "%u%c", &first_id, &dummy) == 1 && first_id < MAX_ENDPOINT_ID) ||
		     sscanf(name, "#%u%c", &first_id, &dummy) == 1) {
	    rc = xprintf(buf, sizeof(buf), " AND id=%u", first_id);
	  } else {
	    rc = xprintf(buf, sizeof(buf), " AND name LIKE %s", str2sql_q(name));
	    if (port)
	      rc = xprintf(buf, sizeof(buf), " AND port=%s", str2sql_q(port));
	  }
	}
	
	if (room)
	    rc = xprintf(buf, sizeof(buf), " AND room LIKE %s", str2sql_q(room));
	
	if (class)
	    rc = xprintf(buf, sizeof(buf), " AND class LIKE %s", str2sql_q(class));
	
	if (comment)
	    rc = xprintf(buf, sizeof(buf), " AND comment LIKE %s", str2sql_q(comment));
    }

    if (order)
	rc = xprintf(buf, sizeof(buf), " ORDER BY %s", order);

    if (rc < 0)
	return -4;
    
    if (sql(buf) < 0)
	return -1;
    
    res = mysql_store_result(mysql);
    if (!res)
	return -2;
    
    fields = mysql_num_fields(res);
    if (fields != 7)
    {
	mysql_free_result(res);
	return -3;
    }

    endpoint_init(&ebuf);
    ep = &ebuf;

    rc = 0;
    while ((row = mysql_fetch_row(res)))
    {
	endpoint_reset(ep);
	
	if (!row[0] || sscanf(row[0], "%u", &ep->id) != 1)
	    continue;

	if (!row[1] || sscanf(row[1], "%u", &ep->type) != 1)
	    continue;

	ep->name = xstrdup(row[2]);
	ep->port = xstrdup(row[3]);
	ep->class = xstrdup(row[4]);
	ep->room = xstrdup(row[5]);
	ep->comment = xstrdup(row[6]);

	rc = (*fun)(ep, xp);
	if (rc)
	    break;
    }

    mysql_free_result(res);
    
    endpoint_reset(ep);
    return rc;
}


int
patch_foreach(int (*fun)(PATCH *pp, void *xp),
	      PLIST *idlist,
	      int type,
	      const char *name,
	      const char *room,
	      const char *class,
	      const char *comment,
	      void *xp,
	      const char *order)
{
    PATCH pbuf, *pp;
    char buf[2048], *tname, *cp;
    char *port = NULL;
    MYSQL_RES *res = NULL;
    MYSQL_ROW row;
    int fields, rc;
    

    if (name)
    {
	tname = xstrdup(name);
	cp = strchr(tname, '@');
	if (cp)
	{
	    *cp++ = '\0';
	    room = cp;
	}
	name = tname;

	port = strchr(tname, ':');
	if (port)
	    *port++ = '\0';

	name = tname;
    }

    buf[0] = 0;
    rc = xprintf(buf, sizeof(buf), "SELECT id,endpoint1,endpoint2,comment,date FROM patches");

    if (idlist || type || name || room || class || comment)
    {
	rc = xprintf(buf, sizeof(buf), " WHERE TRUE");
	
	if (idlist)
	    rc = xprintf(buf, sizeof(buf), " AND id IN (%s)", plist_export(idlist, ",", str2sql_a));
	
	if (type || name || room || class || comment)
	{
	    rc = xprintf(buf, sizeof(buf), " AND ((endpoint1 IN (SELECT id FROM endpoints WHERE TRUE");
	    
	    if (type)
		rc = xprintf(buf, sizeof(buf), " AND type=%d", type);
	    
	    if (name)
	      rc = xprintf(buf, sizeof(buf), " AND name LIKE %s", str2sql_q(name));
	    
	    if (port)
		rc = xprintf(buf, sizeof(buf), " AND port=%s", str2sql_q(port));
	    
	    if (room)
		xprintf(buf, sizeof(buf), " AND room=%s", str2sql_q(room));
	    
	    if (class)
		xprintf(buf, sizeof(buf), " AND class=%s", str2sql_q(class));
	    
	    if (comment)
		xprintf(buf, sizeof(buf), " AND comment LIKE %s", str2sql_q(comment));
	    
	    rc = xprintf(buf, sizeof(buf), ")) OR (endpoint2 IN (SELECT id FROM endpoints WHERE TRUE");
	    
	    if (type)
		rc = xprintf(buf, sizeof(buf), " AND type=%d", type);
	    
	    if (name)
		rc = xprintf(buf, sizeof(buf), " AND name LIKE %s", str2sql_q(name));
	    
	    if (port)
		rc = xprintf(buf, sizeof(buf), " AND port=%s", str2sql_q(port));
	    
	    if (room)
		rc = xprintf(buf, sizeof(buf), " AND room=%s", str2sql_q(room));
	    
	    if (class)
		rc = xprintf(buf, sizeof(buf), " AND class=%s", str2sql_q(class));

	    if (comment)
		rc = xprintf(buf, sizeof(buf), " AND comment LIKE %s", str2sql_q(comment));
	    rc = xprintf(buf, sizeof(buf), ")))");
	}
    }
    
    if (order)
	rc = xprintf(buf, sizeof(buf), " ORDER BY %s", order);

    if (rc < 0)
	return -4;
    
    if (sql(buf) < 0)
	return -1;
    
    res = mysql_store_result(mysql);
    if (!res)
	return -2;
    
    fields = mysql_num_fields(res);
    if (fields != 5)
    {
	mysql_free_result(res);
	return -3;
    }

    patch_init(&pbuf);
    pp = &pbuf;

    rc = 0;
    while ((row = mysql_fetch_row(res)))
    {
	patch_reset(pp);
	
	if (!row[0] || sscanf(row[0], "%u", &pp->id) != 1)
	    continue;

	if (!row[1] || sscanf(row[1], "%u", &pp->p1) != 1)
	    continue;

	if (!row[2] || sscanf(row[2], "%u", &pp->p2) != 1)
	    continue;

	pp->comment = xstrdup(row[3]);
	pp->date = xstrdup(row[4]);

	rc = (*fun)(pp, xp);
	if (rc)
	    break;
    }

    mysql_free_result(res);
    
    patch_reset(pp);
    return rc;
}

char *
endpoint2str(ENDPOINT *ep,
	     char *buf,
	     int bufsize,
	     const char *inroom)
{
    if (!buf)
    {
	buf = malloc(bufsize=512);
	if (!buf)
	    return NULL;
    }
    
    if (!ep)
	*buf = '\0';
    else
    {
	if ((!inroom && !ep->room) ||
	    (inroom && ep->room && strcmp(inroom, ep->room) == 0))
	    snprintf(buf, bufsize, "%s%s%s",
		     ep->name ? ep->name : "",
		     (ep->port && ep->port[0]) ? ":" : "",
		     (ep->port && ep->port[0]) ? ep->port : "");
	else
	    snprintf(buf, bufsize, "%s%s%s@%s",
		     ep->name ? ep->name : "",
		     (ep->port && ep->port[0]) ? ":" : "",
		     (ep->port && ep->port[0]) ? ep->port : "",
		     ep->room ? ep->room : "?");
    }

    return buf;
}


char *
endpoint_type2str(int t, int long_f) {
  char buf[256], *s;

  if (!t)
    return NULL;

  s = buf;
  if (long_f)
    switch (t) {
    case 1:
      return s = "outlet";
    case 2:
      return s = "splitter";
    case 3:
      return s = "equipment";
    default:
      sprintf(buf, "%d", t);
    }
  else 
    switch (t) {
    case 1:
      return s = "OL";
    case 2:
      return s = "SP";
    case 3:
      return s = "EQ";
    default:
      sprintf(buf, "%d", t);
    }
 
  if (long_f > 1)
    s[0] = toupper(s[0]);

  return strdup(s);
}

int
strncasecmp(const char *s1,
	    const char *s2,
	    size_t len) {
  int r = 0;

  while (len-- > 0 && (r = toupper(*s1) - toupper(*s2)) == 0  && *s1 && *s2) {
    ++s1;
    ++s2;
  }

  return r;
}


int
endpoint_str2type(const char *s) {
  int t;


  if (!s)
    return -1;

  if (strncasecmp(s, "OUTLET", strlen(s)) == 0)
    return EP_TYPE_OUTLET;

  if (strncasecmp(s, "SPLITTER", strlen(s)) == 0)
    return EP_TYPE_SPLITTER;

  if (strncasecmp(s, "EQUIPMENT", strlen(s)) == 0)
    return EP_TYPE_EQUIPMENT;

  if (sscanf(s, "%d", &t) == 1 || sscanf(s, "#%d", &t) == 1)
    return t;
  
  return -1;
}


int
db_get_rows(const char *table,
	    const char *where)
{
    MYSQL_RES *res = NULL;
    MYSQL_ROW row;
    char buf[1024];
    int rc, n = -1;
    

    buf[0] = 0;
    if (!where)
	rc = xprintf(buf, sizeof(buf), "SELECT COUNT(id) FROM %s", table);
    else
	rc = xprintf(buf, sizeof(buf), "SELECT COUNT(id) FROM %s WHERE %s", table, where);
    if (rc < 0)
	return -2;
    
    if (sql(buf) < 0)
	return -3;

    res = mysql_store_result(mysql);
    if (!res)
	return -4;
    
    row = mysql_fetch_row(res);
    if (row)
	n = atoi(row[0]);

    mysql_free_result(res);

    return n;
}
