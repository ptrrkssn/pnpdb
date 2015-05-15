#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <locale.h>
#include <netdb.h>
#include <mysql.h>

#include "plist.h"
#include "dbmisc.h"
#include "html.h"
#include "form.h"

#define PATH_CONFIG          "/etc/pnpdb.cfg"

#define TEXT_LIST_PATCHES    "Patches"
#define TEXT_LIST_PORTS      "Ports"
#define TEXT_LIST_OUTLETS    "Outlets"
#define TEXT_LIST_SPLITTERS  "Splitters"
#define TEXT_LIST_SWITCHES   "Switches"

#define TEXT_ADD_PATCH       "Add patch"
#define TEXT_UPDATE_PATCHES  "Update selected patch(es)"
#define TEXT_RESET_PATCHES   "Restore modified patch(es)"
#define TEXT_REMOVE_PATCHES  "Remove selected patch(es)"

#define TEXT_ADD_PORT        "Add port"
#define TEXT_UPDATE_PORTS    "Update selected port(s)"
#define TEXT_RESET_PORTS     "Restore modified port(s)"
#define TEXT_REMOVE_PORTS    "Remove selected port(s)"


extern char version[];

char *path_config = PATH_CONFIG;
char *admins = NULL; /* user,user,user or @netgroup */

char *db_user = "patches.cgi";
char *db_pass = NULL;            /* SQL user password */
char *db_name = "patchdata";
char *db_host = "mysql";

char *title = "Lösenord";

int acl_update = 0;


char *remote_user;
char *remote_addr;

char *request_uri;
char *request_url;

char *action = NULL;
char *order = NULL;

time_t now;
int verbose = 0;
int debug = 0;
int nowrite = 0;

char port[256];
int start = 1;
int end = 1;
unsigned int type = 0;
char *class = NULL;
char *name = NULL;
char *room = NULL;
char *comment = NULL;

char *pprefix = NULL;
int pfill = 0;

static int en = 0;
static int pn = 0;

int maxlist = 0;
int g_errflag = 0;


int
is_admin_user(const char *user)
{
    char *buf, *cp;

    
    if (!user)
	return 0;
    
    if (!admins)
	return 1;

    buf = strdup(admins);
    if (!buf)
	return 0;

    cp = strtok(buf, ",");
    while (cp)
    {
	if (*cp == '@')
	{
	    if (innetgr(cp+1, NULL, user, NULL))
	    {
		free(buf);
		return 1;
	    }
	}
	else
	{
	    if (strcmp(cp, user) == 0)
	    {
		free(buf);
		return 1;
	    }
	}

	cp = strtok(NULL, ",");
    }

    free(buf);
    return 0;
}



int
print_port(ENDPOINT *ep,
	   void *xp)
{
    PATCH pbuf;
    int pid;

    ++en;

    if (maxlist && en > maxlist)
	return 0;
    
    h_tr_start(NULL);

    h_td_start("action", 1);
    printf("<input type=\"checkbox\" name=\"eid\" value=\"%u\">", ep->id);
    h_td_end();

    h_td_start("id", 1);
    printf("%u", ep->id);
    h_td_end();
    
    h_td_start("name", 1);
    if (ep->name)
    {
	h_a_start("href", "Show ports", "?action=list-ports&name=%s", ep->name);
	h_puts(ep->name);
	h_a_end();
	if (ep->port && ep->port[0])
	{
	    h_puts(":");
	    h_a_start("href", "Show ports", "?action=list-ports&name=%s:%s",
		      ep->name, ep->port);
	    h_puts(ep->port);
	    h_a_end();
	}
    }
    else
	h_puts(NULL);
    h_td_end();

    h_td_start("room", 1);
    if (ep->room)
	h_a_start("href", "Show ports", "?action=list-ports&room=%s", ep->room);
    h_puts(ep->room);
    if (ep->room)
	h_a_end();
    h_td_end();

    h_td_start("type", 1);
    switch (ep->type)
    {
      case 1:
	h_puts("Outlet");
	break;

      case 2:
	h_puts("Splitter");
	break;

      case 3:
	h_puts("Switch");
	break;

      default:
	printf("#%d", ep->type);
    }
    h_td_end();
		   
    h_td_str(ep->class, "class", 1);
    h_td_str(ep->comment, "comment", 1);
    
    h_td_start("patch", 1);
    pid = patch_lookup_endpoint(&pbuf, ep->id);
    if (pid > 0)
    {
	h_a_start("href", "Show patch", "?action=list-patches&pid=%d", pid);
	printf("%d", pid);
	h_a_end();
	
	++pn;
    }
    else if (pid == 0)
    {
	if (acl_update)
	{
	    h_a_start("href", "Add patch", "?action=add-patch&destL=%d", ep->id);
	    h_puts("[Add]");
	    h_a_end();
	}

    }
    else if (pid == -4)
    {
	h_a_start("href", "Show patches", "?action=list-patches&name=%s%s%s",
		  ep->name,
		  (ep->port && ep->port[0]) ? ":" : "",
		  ep->port ? ep->port : "");
	h_puts("???");
	h_a_end();
    }
    else
    {
	printf("??? <!-- error=%d -->", pid);
    }
    h_td_end();

    h_td_start("action", 1);
    if (pid > 0)
	printf("<input type=\"checkbox\" name=\"pid\" value=\"%u\">", pid);
    else
	h_puts(NULL);
    h_td_end();
	
    h_tr_end();

    return 0;
}
	       

void
print_type_select(const char *name,
		  unsigned int type)
{
    static char *types[] = { "- Select -", "Outlet", "Splitter", "Switch" };
    int i;

    
    printf("<select name=\"%s\">\n", name);
    for (i = 0; i < 4; i++)
    {
	printf("<option value=\"%d\" %s>%s</option>\n",
	       i,
	       (i == type) ? "selected" : "",
	       types[i]);
    }
    puts("</select>");
}


void
print_port_header(int with_patch)
{
    h_tr_start("header");
    if (with_patch < 2)
	h_th_str("", "action", 1);

    if (with_patch != 2)
	h_th_str("Id", "id", 1);
    
    h_th_str("Name", "name", 1);
    h_th_str("Room", "room", 1);
    h_th_str("Type", "type", 1);
    h_th_str("Class", "class", 1);
    h_th_str("Comment", "comment", 1);
    
    if (with_patch == 1)
	h_th_str("Patch", "patch", 2);
    h_tr_end();
}



int
list_ports(int argc,
	   char *argv[])
{
    char *pidstr = NULL;
    PLIST *pidlist = NULL;
    

    if (form_gets("pid", &pidstr) == 1)
	pidlist = plist_parse(NULL, pidstr, " ,");
    
    h_header("Search");
    
    h_form_start("get", request_url);

    puts("<h3>Filters</h3>");

    puts("Patch: ");
    h_form_str("pid", "text", 16, plist_export(pidlist, ",", NULL));
    
    puts(" Port: ");
    h_form_str("name", "text", 16, name);
    
    puts(" Room: ");
    h_form_str("room", "text", 16, room);
    
    puts(" Class: ");
    h_form_str("class", "text", 16, class);
    
    puts(" Comment: ");
    h_form_str("comment", "text", 32, comment);

    puts(" Max: ");
    if (maxlist)
	h_form_int("max", "text", 4, maxlist);
    else
	h_form_str("max", "text", 4, "");

    puts("\n<h3>Show</h3>");
    
    h_button_submit("action", "list-patches",   TEXT_LIST_PATCHES);
    h_button_submit("action", "list-ports",     TEXT_LIST_PORTS);
    h_button_submit("action", "list-outlets",   TEXT_LIST_OUTLETS);
    h_button_submit("action", "list-splitters", TEXT_LIST_SPLITTERS);
    h_button_submit("action", "list-switches",  TEXT_LIST_SWITCHES);
    
    if (!pidlist && !type && !name && !room && !class && !comment)
	return 0;

    if (name && !*name)
	name = NULL;
    if (room && !*room)
	room = NULL;
    if (class && !*class)
	class = NULL;
    if (comment && !*comment)
	comment = NULL;
    
    puts("<h3>Ports</h3>");

    h_table_start("endpoints");
    print_port_header(1);

    en = 0;
    pn = 0;
    endpoint_foreach(print_port, pidlist, type, name, room, class, comment, NULL, order ? order : "name,port+0,port");
    h_table_end();

    h_table_start("footer");
    h_tr_start("footer");
    
    h_td_start("selectL", 1);
    if (en > 1)
	fputs("<input type=\"button\" value=\"Select all ports\" onClick=\"this.value=toggle(this.form.eid,true,' ports')\">", stdout);
    h_td_end();
    
    h_td_start("summary", 1);
    printf("%d port%s found",en, en != 1 ? "s" : "");
    if (maxlist && en > maxlist)
	printf(" (%d shown)", maxlist);
    h_td_end();
    
    h_td_start("selectR", 1);
    if (pn > 1)
	fputs("<input type=\"button\" value=\"Select all patches\" onClick=\"this.value=toggle(this.form.pid,true,' patches')\">", stdout);
    h_td_end();
    
    h_tr_end();
    h_table_end();

    if (acl_update)
    {
	if (en > 0 || pn > 0)
	    puts("<h3>Action</h3>");
	
	if (en > 0)
	{
	    h_button_submit("action", "update-ports",   TEXT_UPDATE_PORTS);
	    h_button_submit("action", "remove-ports",   TEXT_REMOVE_PORTS);
	}
	
	if (pn > 0)
	{
	    h_button_submit("action", "update-patches",   TEXT_UPDATE_PATCHES);
	    h_button_submit("action", "remove-patches",   TEXT_REMOVE_PATCHES);
	}
    }
    h_form_end();

    return 0;
}






int
print_patch(PATCH *pp,
	    void *xp)
{
    ENDPOINT eb1, eb2;
    int eid1, eid2;
    char *destL = NULL;
    char *destR = NULL;
    char *dLroom = NULL;
    char *dRroom = NULL;
    char *troom = NULL;
    
    
    ++pn;

    if (maxlist && pn > maxlist)
	return 0;
    
    eid1 = endpoint_lookup_id(&eb1, pp->p1);
    eid2 = endpoint_lookup_id(&eb2, pp->p2);

    h_tr_start(NULL);

    h_td_start("action", 1);
    printf("<input type=\"checkbox\" name=\"pid\" value=\"%u\">", pp->id);
    h_td_end();

    h_td_start("id", 1);
    h_a_start("href", "Show ports", "?action=list-ports&pid=%d", pp->id);
    printf("%d", pp->id);
    h_a_end();
    h_td_end();
    
    if (eid1 > 0)
	destL = endpoint2str(&eb1, NULL, 0, NULL);

    if (eid2 > 0)
	destR = endpoint2str(&eb2, NULL, 0, NULL);

    if (destL)
    {
	en++;
	dLroom = strchr(destL, '@');
    }

    if (destR)
    {
	en++;
	dRroom = strchr(destR, '@');
    }

    printf("<!-- dLroom=%s, dRroom=%s -->\n", dLroom ? dLroom : "NULL", dRroom ? dRroom : "NULL");
    
    if (dLroom)
    {
	troom = strdup(dLroom+1);
	if (dRroom && strcmp(dLroom, dRroom) == 0)
	    *dRroom = 0;
	*dLroom = 0;
    }
    else if (dRroom)
    {
	troom = strdup(dRroom+1);
	*dRroom = 0;
    }
    else
	troom = room;

    h_td_start("room", 1);
    if (troom)
    {
	h_a_start("href", "Show patches", "?action=list-patches&room=%s", troom);
	h_puts(troom);
	h_a_end();
    }
    h_td_end();

    h_td_start("destL", 1);
    if (destL)
    {
	h_a_start("href", "Show port", "?action=list-ports&name=%s", destL);
	h_puts(destL);
	h_a_end();
    }
    else
    {
	if (pp->p1)
	    printf("?<!-- #%d -->", pp->p1);
	else
	    fputs("&nbsp;", stdout);
    }
    h_td_end();
    
    h_td_str("-", "destC", 1);
    
    h_td_start("destR", 1);
    if (destR)
    {
	h_a_start("href", "Show port", "?action=list-ports&name=%s", destR);
	h_puts(destR);
	h_a_end();
    }
    else
    {
	if (pp->p2)
	    printf("?<!-- %d -->", pp->p2);
	else
	    fputs("&nbsp;", stdout);
    }
    h_td_end();
    
    
    h_td_str(pp->comment, "comment", 1);
    h_td_str(pp->date, "date", 1);

    h_tr_end();

    return 0;
}
	       
void
print_patch_header(int actflag)
{
    h_tr_start("header");
    if (actflag)
	h_th_str("", "action", 1);
    h_th_str("Id", "id", 1);
    h_th_str("Room", "room", 1);
    h_th_str("Between", "dest", 3);
    h_th_str("Comment", "comment", 1);
    if (actflag)
    {
	h_th_str("Updated", "date", 1);
#if 0
	h_th_str("", "modified", 1);
#endif
    }
    h_tr_end();
}

int
print_patch_update_form(int id,
			int row)
{
    PATCH pbuf;
    ENDPOINT eb1, eb2;
    char *destL = NULL;
    char *destR = NULL;
    char *comment = NULL;
    char *date = NULL;
    char *troom = NULL;
    int errflag = 0;
    int modified = 0;
    char *dLroom = NULL;
    char *dRroom = NULL;
    
    
    patch_init(&pbuf);
    
    if (patch_lookup_id(&pbuf, id) != id)
	errflag |= 0x0001;
    else
    {
	if (endpoint_lookup_id(&eb1, pbuf.p1) > 0)
	    destL = endpoint2str(&eb1, NULL, 0, NULL);
	
	if (endpoint_lookup_id(&eb2, pbuf.p2) > 0)
	    destR = endpoint2str(&eb2, NULL, 0, NULL);
	
	comment = pbuf.comment;
	date = pbuf.date;
    }

    printf("<!-- destL=%s, destR=%s -->\n", destL ? destL : "NULL", destR ? destR : "NULL");
    
    h_form_set_row(row);
    form_set_row(row);

    troom = room;
    form_gets("room", &troom);
    
    switch (form_gets("destL", &destL))
    {
      case 1:
	if (endpoint_lookup(&eb1, destL, troom) <= 0)
	    errflag |= 0x0010;
	else
	{
	    destL = endpoint2str(&eb1, NULL, 0, NULL);
	    
	    if (eb1.id != pbuf.p1)
	    {
		if (patch_update_endpoint(pbuf.id, 1, eb1.id) != 1)
		    errflag |= 0x0020;
		else
		{
		    pbuf.p1 = eb1.id;
		    modified = 1;
		}
	    }
	}
	break;

      case 0:
	if (pbuf.p1)
	{
	    if (patch_update_endpoint(pbuf.id, 1, 0) != 1)
		errflag |= 0x0040;
	    else
	    {
		pbuf.p1 = 0;
		modified = 1;
	    }
	}
    }

    switch (form_gets("destR", &destR))
    {
      case 1:
	if (endpoint_lookup(&eb2, destR, troom) <= 0)
	    errflag |= 0x0100;
	else
	{
	    destR = endpoint2str(&eb2, NULL, 0, NULL);
	    
	    if (eb2.id != pbuf.p2)
	    {
		if (patch_update_endpoint(pbuf.id, 2, eb2.id) != 1)
		    errflag |= 0x0200;
		else
		{
		    pbuf.p2 = eb2.id;
		    modified = 1;
		}
	    }
	}
	break;

      case 0:
	if (pbuf.p2)
	{
	    if (patch_update_endpoint(pbuf.id, 2, 0) != 1)
		errflag |= 0x0400;
	    else
	    {
		pbuf.p2 = 0;
		modified = 1;
	    };
	}
    }
    
    if (destL)
	dLroom = strchr(destL, '@');

    if (destR)
	dRroom = strchr(destR, '@');

    printf("<!-- dLroom=%s, dRroom=%s -->\n", dLroom ? dLroom : "NULL", dRroom ? dRroom : "NULL");
    
    if (dLroom)
    {
	troom = strdup(dLroom+1);
	if (dRroom && strcmp(dLroom, dRroom) == 0)
	    *dRroom = 0;
	*dLroom = 0;
    }
    else if (dRroom)
    {
	troom = strdup(dRroom+1);
	*dRroom = 0;
    }
    else
	troom = room;

    switch (form_gets("comment", &comment))
    {
      case 1:
	if (!pbuf.comment || strcmp(pbuf.comment, comment) != 0)
	{
	    if (patch_update_comment(pbuf.id, comment) != 1)
		errflag |= 0x1000;
	    else
	    {
		pbuf.comment = comment;
		modified = 1;
	    }
	}
	break;

      case 0:
	if (pbuf.comment && pbuf.comment[0])
	{
	    if (patch_update_comment(pbuf.id, NULL) != 1)
		errflag |= 0x2000;
	    else
	    {
		pbuf.comment = NULL;
		modified = 1;
	    }
	}
    }

    if (errflag)
	modified = 0;

    h_tr_start(NULL);
    
    h_td_start("action", 1);
    printf("<input type=\"checkbox\" name=\"pid\" value=\"%u\" checked>", id);
    h_td_end();

    h_td_start("id", 1);
    h_a_start("href", "Show patch", "?action=list-patches&pid=%d", id);
    printf("%d", id);
    h_a_end();
    h_form_int("pid", "hidden", 0, id);
    h_td_end();
    
    h_td_start("room", 1);
    h_a_start("href", "Show patches", "?action=list-patches&room=%s", troom);
    h_puts(troom);
    h_a_end();
    h_form_str("room", "hidden", 10, troom);
    h_td_end();

    h_td_start("destL", 1);
    h_form_str("destL", "text", 20, destL);
    h_td_end();

    h_td_start("destC", 1);
    putchar('-');
    h_td_end();
    
    h_td_start("destR", 1);
    h_form_str("destR", "text", 20, destR);
    h_td_end();
    
    h_td_start("comment", 1);
    h_form_str("comment", "text", 30, comment);
    h_td_end();

    h_td_start("date", 1);
    h_puts(pbuf.date);
    h_td_end();

    if (errflag || modified)
    {
	h_td_start("state", 1);
	if (errflag)
	{
	    printf("<blink><font color=\"red\" size=\"+1\">*</font></blink>");
	    if (debug)
		printf("<!-- error=0x%04x -->", errflag);
	}
	if (modified)
	    fputs("<blink><font color=\"green\" size=\"+1\">!</font></blink>", stdout);
	h_td_end();
    }
    
    h_tr_end();
    
    h_form_set_row(0);
    form_set_row(0);

    ++pn;
	
    if (errflag)
    {
	++g_errflag;
	return -errflag;
    }

    return modified;
}


int
patch_form_get(PATCH *pp,
	       int row)
{
    ENDPOINT eb1, eb2;
    char *destL = NULL;
    char *destR = NULL;
    char *comment = NULL;
    char *dLroom = NULL;
    char *dRroom = NULL;
    char *troom = NULL;
    
    int got_id = 0;
    int got_destL = 0;
    int got_destR = 0;
    int got_comment = 0;
    int errflag = 0;
    
    form_set_row(row);

    patch_init(pp);
    
    got_id = form_geti("pid", &pp->id);
    if (got_id == 1)
    {
	if (pp->id < 1 || pp->id > MAX_PATCH_ID)
	    errflag |= 0x0010;
    }

    got_destL = form_gets("destL", &destL);
    if (got_destL == 1)
    {
	if (endpoint_lookup(&eb1, destL, room) <= 0)
	    errflag |= 0x0020;
	else
	{
	    destL = endpoint2str(&eb1, NULL, 0, NULL);
	    pp->p1 = eb1.id;
	}
    }
    
    got_destR = form_gets("destR", &destR);
    if (got_destR == 1)
    {
	if (endpoint_lookup(&eb2, destR, room) <= 0)
	    errflag |= 0x0040;
	else
	{
	    destR = endpoint2str(&eb2, NULL, 0, NULL);
	    pp->p2 = eb2.id;
	}
    }
    if (destL)
	dLroom = strchr(destL, '@');

    if (destR)
	dRroom = strchr(destR, '@');

    if (dLroom)
    {
	troom = strdup(dLroom+1);
	if (dRroom && strcmp(dLroom, dRroom) == 0)
	    *dRroom = 0;
	*dLroom = 0;
    }
    else if (dRroom)
    {
	troom = strdup(dRroom+1);
	*dRroom = 0;
    }
    else
	troom = room;

    got_comment = form_gets("comment", &comment);
    pp->comment = comment ? comment : "";

    form_set_row(0);
    
    if (errflag)
    {
	++g_errflag;
	return -errflag;
    }
    
    if (got_id == 1)
    {
	if (got_destL == 1 || got_destR == 1)
	    return 2;

	return 1;
    }

    return 0;
}


int
print_patch_add_form(PATCH *pp)
{
    ENDPOINT eb1, eb2;
    char *destL = NULL;
    char *destR = NULL;
    int errflag = 0;
    char *dLroom = NULL;
    char *dRroom = NULL;
    char *troom = NULL;
    

    if (pp->p1)
    {
	if (endpoint_lookup_id(&eb1, pp->p1) <= 0)
	    errflag = 1;
	else
	    destL = endpoint2str(&eb1, NULL, 0, NULL);
    }

    if (pp->p2)
    {
	if (endpoint_lookup_id(&eb2, pp->p2) <= 0)
	    errflag = 1;
	else
	    destR = endpoint2str(&eb2, NULL, 0, NULL);
    }

    if (destL)
	dLroom = strchr(destL, '@');

    if (destR)
	dRroom = strchr(destR, '@');

    if (dLroom)
    {
	troom = strdup(dLroom+1);
	if (dRroom && strcmp(dLroom, dRroom) == 0)
	    *dRroom = 0;
	*dLroom = 0;
    }
    else if (dRroom)
    {
	troom = strdup(dRroom+1);
	*dRroom = 0;
    }
    else
	troom = room;

    if (pp->id)
    {
	if (pp->id < 1 || pp->id > MAX_PATCH_ID)
	    errflag = 1;
    }
    else if (troom)
    {
	int hid = patch_find_highest(troom); 

	if (hid > 0)
	    pp->id = hid+1;
    }

    h_tr_start(NULL);
    
    h_td_start("id", 1);
    if (pp->id != 0)
	h_form_int("pid", "text", 6, pp->id);
    else
	h_form_str("pid", "text", 6, "");
    h_td_end();
    
    h_td_start("room", 1);
    h_form_str("room", "text", 10, troom);
    h_td_end();

    h_td_start("destL", 1);
    h_form_str("destL", "text", 20, destL);
    h_td_end();

    h_td_start("destC", 1);
    putchar('-');
    h_td_end();
    
    h_td_start("destR", 1);
    h_form_str("destR", "text", 20, destR);
    h_td_end();
    
    h_td_start("comment", 1);
    h_form_str("comment", "text", 30, pp->comment);
    h_td_end();

    h_td_start("date", 1);
    h_puts(pp->date);
    h_td_end();

    if (errflag)
    {
	h_td_start("state", 1);
	if (errflag)
	    fputs("<blink><font color=\"red\" size=\"+1\">*</font></blink>", stdout);
	h_td_end();
    }
    
    h_tr_end();
    
    if (errflag)
    {
	++g_errflag;
	return -errflag;
    }

    return pp->id;
}




int
remove_patch(int argc,
	     char *argv[])
{
    unsigned int id;
    PATCH pbuf;
    
    
    h_header("Remove patch");
    
    if (form_getu("pid", &id) != 1)
    {
	puts("Error: Need patch id");
	return 0;
    }

    if (patch_lookup_id(&pbuf, id) != id)
    {
	printf("No such patch (%d) found.", id);
	return 0;
    }
    
    if (patch_remove(id) == 1)
	printf("Patch %d removed from database.", id);
    else
	printf("An error occured while removing patch id %d\n", id);

    return 0;
}



int
print_port_update_form(int id,
		       int row)
{
    ENDPOINT ebuf;
    int errflag = 0;
    int modified = 0;

    char *pname = NULL;
    
    char *name = NULL;
    char *room = NULL;
    unsigned int type = 0;
    char *class = NULL;
    char *comment;
    
    PATCH pbuf;

    
    endpoint_init(&ebuf);
    
    if (endpoint_lookup_id(&ebuf, id) != id)
	errflag |= 0x0001;
    else
	pname = endpoint2str(&ebuf, NULL, 0, ebuf.room);

    
    h_form_set_row(row);
    form_set_row(row);
    
    switch (form_gets("name", &name))
    {
      case 1:
	if (!pname || strcmp(name, pname) != 0)
	{
	    if (endpoint_update_name(ebuf.id, name) != 1)
		errflag |= 0x0010;
	    else
	    {
		pname = name;
		modified = 1;
	    }
	}
	break;

      case 0:
	if (pname && *pname)
	{
	    if (endpoint_update_name(ebuf.id, NULL) != 1)
		errflag |= 0x0020;
	    else
	    {
		pname = NULL;
		modified = 1;
	    }
	}
    }
	    
    switch (form_gets("room", &room))
    {
      case 1:
	if (!ebuf.room || strcmp(room, ebuf.room) != 0)
	{
	    if (endpoint_update_room(ebuf.id, room) != 1)
		errflag |= 0x0040;
	    else
	    {
		ebuf.room = room;
		modified = 1;
	    }
	}
	break;

      case 0:
	if (ebuf.room && ebuf.room[0])
	{
	    if (endpoint_update_room(ebuf.id, NULL) != 1)
		errflag |= 0x0080;
	    else
	    {
		ebuf.room = NULL;
		modified = 1;
	    }
	}
    }

    switch (form_getu("type", &type))
    {
      case 1:
	if (!ebuf.type || type != ebuf.type)
	{
	    if (endpoint_update_type(ebuf.id, type) != 1)
		errflag |= 0x0100;
	    else
	    {
		ebuf.type = type;
		modified = 1;
	    }
	}
	break;

      case 0:
	if (ebuf.type)
	{
	    if (endpoint_update_type(ebuf.id, 0) != 1)
		errflag |= 0x0200;
	    else
	    {
		ebuf.type = 0;
		modified = 1;
	    }
	}
    }

    switch (form_gets("class", &class))
    {
      case 1:
	if (!ebuf.class || strcmp(class, ebuf.class) != 0)
	{
	    if (endpoint_update_class(ebuf.id, class) != 1)
		errflag |= 0x0400;
	    else
	    {
		ebuf.class = class;
		modified = 1;
	    }
	}
	break;

      case 0:
	if (ebuf.class && ebuf.class[0])
	{
	    if (endpoint_update_class(ebuf.id, NULL) != 1)
		errflag |= 0x0800;
	    else
	    {
		ebuf.class = NULL;
		modified = 1;
	    }
	}
    }

    switch (form_gets("comment", &comment))
    {
      case 1:
	if (!ebuf.comment || strcmp(comment, ebuf.comment) != 0)
	{
	    if (endpoint_update_comment(ebuf.id, comment) != 1)
		errflag |= 0x0040;
	    else
	    {
		ebuf.comment = comment;
		modified = 1;
	    }
	}
	break;

      case 0:
	if (ebuf.comment && ebuf.comment[0])
	{
	    if (endpoint_update_comment(ebuf.id, NULL) != 1)
		errflag |= 0x0080;
	    else
	    {
		ebuf.comment = NULL;
		modified = 1;
	    }
	}
    }

    
    h_tr_start(NULL);

    h_td_start("action", 1);
    printf("<input type=\"checkbox\" name=\"eid\" value=\"%u\" checked>", ebuf.id);
    h_td_end();

    h_td_start("id", 1);
    printf("%u", ebuf.id);
    h_td_end();
    
    h_td_start("name", 1);
    h_form_str("name", "text", 10, pname);
    h_td_end();

    h_td_start("room", 1);
    h_form_str("room", "text", 10, ebuf.room);
    h_td_end();

    h_td_start("type", 1);
    print_type_select("type", ebuf.type);
    h_td_end();

    h_td_start("class", 1);
    h_form_str("class", "text", 5, ebuf.class);
    h_td_end();

    h_td_start("comment", 1);
    h_form_str("comment", "text", 40, ebuf.comment);
    h_td_end();

    h_td_start("patch", 1);
    if (patch_lookup_endpoint(&pbuf, ebuf.id))
    {
	h_a_start("href", "Show patch", "?action=list-patches&pid=%u", pbuf.id);
	printf("%u", pbuf.id);
	h_a_end();
    }
    else
	fputs("&nbsp;", stdout);
    h_td_end();


    if (errflag || modified)
    {
	h_td_start("state", 1);
	if (errflag)
	{
	    printf("<blink><font color=\"red\" size=\"+1\">*</font></blink>");
	    if (debug)
		printf("<!-- error=0x%04x -->", errflag);
	}
	if (modified)
	    fputs("<blink><font color=\"green\" size=\"+1\">!</font></blink>", stdout);
	h_td_end();
    }
    
    h_tr_end();
    
    h_form_set_row(0);
    form_set_row(0);

    ++en;
	
    if (errflag)
    {
	++g_errflag;
	return -errflag;
    }

    return modified;
}

int
update_ports(int argc,
	     char *argv[])
{
    int rc, i;
    unsigned int id = 0;
    unsigned int modified = 0;
    unsigned int errors = 0;
    char *idstr = NULL;
    PLIST *idlist = NULL;
    

    if (!acl_update)
    {
	h_header("Access denied");
	puts("Your user access level does not allow you to perform this operation.");
	return 0;
    }
    
    h_header("Ports update", version);

    if (form_gets("eid", &idstr) != 1 || !(idlist = plist_parse(NULL, idstr, " ,")))
    {
	puts("Error: Need port id(s)");
	return 0;
    }


    
    h_form_start("post", request_url);

    h_table_start("ports");
    print_port_header(1);

    g_errflag = 0;
    en = 0;
    for (i = 0; i < plist_length(idlist); i++)
	if (sscanf(plist_get(idlist, i), "%u", &id) == 1)
	{
	    rc = print_port_update_form(id, id);
	    if (rc < 0)
		++errors;
	    else if (rc > 0)
		++modified;
	}

    h_table_end();
    
    h_table_start("footer");
    h_tr_start("footer");
    h_td_start("selectL", 1);
    if (en > 1)
	fputs("<input type=\"button\" value=\"Unselect all\" onClick=\"this.value=toggle(this.form.eid,false,'')\">", stdout);
    h_td_end();
    h_td_start("summary", 1);
    printf("%d port%s found",
	   en, en ? "s" : "");
    h_td_end();
    h_td_start("selectR", 1);
    h_puts(NULL);
    h_td_end();
    h_tr_end();
    h_table_end();
    
    if (g_errflag)
	puts("<p><i>Please note: Invalid data in one or more fields detected.</i>\n");

    if (en > 0)
    {
	puts("<h3>Action</h3>");
	h_button_submit("action", "update-ports", TEXT_UPDATE_PORTS);
	h_button_reset(TEXT_RESET_PORTS);
	h_button_submit("action", "remove-ports", TEXT_REMOVE_PORTS);
    }
    h_form_end();

    if (modified || errors)
	puts("<h4>Please note:</h4>");
    
    if (modified > 0)
	printf("<i>%d record%s have been updated (marked by a flashing green '!')<br>",
	       modified,
	       modified == 1 ? "" : "s");
    
    if (errors > 0)
	printf("<i>%d record%s contain one or more invalid field, or the database could not be updated (marked by a flashing red '*')</i><br>",
	       errors,
	       errors == 1 ? "" : "s");

    
    plist_free(idlist);
    return 0;
}



int
update_patches(int argc,
	       char *argv[])
{
    int rc, i;
    unsigned int id = 0;
    unsigned int modified = 0;
    unsigned int errors = 0;
    char *idstr = NULL;
    PLIST *idlist = NULL;

    if (!acl_update)
    {
	h_header("Access denied");
	puts("Your user access level does not allow to to perform this operation.");
	return 0;
    }
    
    h_header("Patch update");
    
    if (form_gets("pid", &idstr) != 1 || !(idlist = plist_parse(NULL, idstr, ", ")))
    {
	puts("Error: Need patch id(s)");
	return 0;
    }

    h_form_start("post", request_url);

    h_table_start("patches");
    print_patch_header(1);

    g_errflag = 0;
    pn = 0;
    for (i = 0; i < plist_length(idlist); i++)
	if (sscanf(plist_get(idlist, i), "%u", &id) == 1)
	{
	    rc = print_patch_update_form(id, id);
	    if (rc < 0)
		++errors;
	    else if (rc > 0)
		++modified;
	}
    
    h_table_end();
    
    h_table_start("footer");
    h_tr_start("footer");
    h_td_start("selectL", 1);
    if (pn > 1)
	fputs("<input type=\"button\" value=\"Unselect all\" onClick=\"this.value=toggle(this.form.pid,false,'')\">", stdout);
    h_td_end();
    h_td_start("summary", 1);
    printf("%d patch%s found", pn, pn != 1 ? "es" : "");
    if (maxlist && pn > maxlist)
	printf(" (%d shown)", maxlist);
    h_td_end();
    h_td_start("selectR", 1);
    h_puts(NULL);
    h_td_end();
    h_tr_end();
    h_table_end();

    if (g_errflag)
	puts("<p><i>Please note: Invalid data in one or more fields detected.</i>\n");

    if (acl_update)
    {
	if (pn > 0)
	{
	    puts("<h3>Action</h3>");
	    h_button_submit("action", "update-patches", TEXT_UPDATE_PATCHES);
	    h_button_reset(TEXT_RESET_PATCHES);
	    h_button_submit("action", "remove-patches",   TEXT_REMOVE_PATCHES);
	}
    }
    h_form_end();

    if (modified || errors)
	puts("<h4>Please note:</h4>");
    
    if (modified > 0)
	printf("<i>%d record%s have been updated (marked by a flashing green '!')<br>",
	       modified,
	       modified == 1 ? "" : "s");
    
    if (errors > 0)
	printf("<i>%d record%s contain one or more invalid field, or the database could not be updated (marked by a flashing red '*')</i><br>",
	       errors,
	       errors == 1 ? "" : "s");

    plist_free(idlist);
    return 0;
}


int
add_patch(int argc,
	  char *argv[])
{
    int rc;
    PATCH pbuf;
    char *confirm = NULL;
    

    if (!acl_update)
    {
	h_header("Access denied");
	puts("Your user access level does not allow to to perform this operation.");
	return 0;
    }
    
    form_gets("confirm", &confirm);

    rc = patch_form_get(&pbuf, 0);
    if (rc > 0 && confirm && strcmp(confirm, "yes") == 0)
    {
	rc = patch_insert(&pbuf);
	if (rc != 1)
	{
	    h_header("Patch add failed");
	    printf("Failure adding patch %d (code: %d): ", pbuf.id, rc);
	    h_puts(db_error());
	}
	else
	{
	    h_header("Patch added");
	    h_table_start("patches");
	    print_patch_header(1);
	    patch_lookup_id(&pbuf, pbuf.id);
	    print_patch(&pbuf, NULL);
	    h_table_end();

	    puts("<h3>Action</h3>");
	    h_button_submit("action", "update-patches",   TEXT_UPDATE_PATCHES);
	    h_button_reset(TEXT_RESET_PATCHES);
	    h_button_submit("action", "remove-patches",   TEXT_REMOVE_PATCHES);
	}
    }
    else
    {
	h_header("Patch add");
	
	h_form_start("get", request_url);
	g_errflag = 0;
	h_table_start("patches");
	print_patch_header(0);
	rc = print_patch_add_form(&pbuf);
	h_table_end();
	if (g_errflag)
	    puts("<p><i>Please note: Invalid data in one or more fields detected.</i>\n");
	h_form_str("confirm", "hidden", 0, "yes");
	
	puts("<h3>Action</h3>");
	h_button_submit("action", "add-patch", TEXT_ADD_PATCH);
	h_form_end();
    }
    
    return 0;
}




int
list_patches(int argc,
	     char *argv[])
{
    char *pidstr = NULL;
    PLIST *pidlist = NULL;

    
    h_header("Search");

    if (form_gets("pid", &pidstr) == 1)
	pidlist = plist_parse(NULL, pidstr, " ,");
    
    h_form_start("get", request_url);
    
    puts("<h3>Filters</h3>");

    puts("Patch: ");
    h_form_str("pid", "text", 16, plist_export(pidlist, ",", NULL));
    
    puts(" Port: ");
    h_form_str("name", "text", 16, name);
    
    puts(" Room: ");
    h_form_str("room", "text", 16, room);
    
    puts(" Class: ");
    h_form_str("class", "text", 16, class);

    puts(" Comment: ");
    h_form_str("comment", "text", 32, comment);

    puts(" Max: ");
    if (maxlist)
	h_form_int("max", "text", 4, maxlist);
    else
	h_form_str("max", "text", 4, "");

    puts("\n<h3>Show</h3>");
    
    h_button_submit("action", "list-patches",   TEXT_LIST_PATCHES);
    h_button_submit("action", "list-ports",     TEXT_LIST_PORTS);
    h_button_submit("action", "list-outlets",   TEXT_LIST_OUTLETS);
    h_button_submit("action", "list-splitters", TEXT_LIST_SPLITTERS);
    h_button_submit("action", "list-switches",  TEXT_LIST_SWITCHES);
    
    if (!pidlist && !name && !room && !class && !comment)
    {
	h_form_end();
	return 0;
    }

    if (name && !*name)
	name = NULL;
    if (room && !*room)
	room = NULL;
    if (class && !*class)
	class = NULL;
    if (comment && !*comment)
	comment = NULL;
    
    puts("<h3>Patches</h3>");
    
    h_table_start("patches");
    print_patch_header(1);

    pn = 0;
    patch_foreach(print_patch, pidlist, type, name, room, class, comment, (char *) argv[1], order ? order : "id");

    h_table_end();

    h_table_start("footer");
    h_tr_start("footer");
    h_td_start("selectL", 1);
    if (pn > 1)
	puts("<input type=\"button\" value=\"Select all\" onClick=\"this.value=toggle(this.form.pid,true,'')\">");
    h_td_end();
    h_td_start("summary", 1);
    printf("%d patch%s found", pn, pn != 1 ? "es" : "");
    if (maxlist && pn > maxlist)
	printf(" (%d shown)", maxlist);
    h_td_end();
    h_td_start("selectR", 1);
    h_puts(NULL);
    h_td_end();
    h_tr_end();
    h_table_end();
    
    if (acl_update)
    {
	if (pn > 0)
	{
	    puts("<h3>Action</h3>");
	    h_button_submit("action", "update-patches",   TEXT_UPDATE_PATCHES);
	    h_button_submit("action", "remove-patches",   TEXT_REMOVE_PATCHES);
	}
    }
    h_form_end();
	
    return 0;
}



int
remove_patches(int argc,
	     char *argv[])
{
    int rc, i;
    unsigned int id = 0;
    unsigned int errors = 0;
    char *idstr = NULL;
    PLIST *idlist = NULL;
    char *confirm = NULL;
    

    if (form_gets("pid", &idstr) != 1 || !(idlist = plist_parse(NULL, idstr, " ,")))
    {
	h_header("Error removing patches");
	
	puts("Error: Need patch id(s)");
	return 0;
    }
	    
    form_gets("confirm", &confirm);
    if (confirm && strcmp(confirm, "yes") == 0)
    {
	h_header("Removing patches");

	pn = 0;
	puts("<ul>");
	for (i = 0; i < plist_length(idlist); i++)
	    if (sscanf(plist_get(idlist, i), "%u", &id) == 1)
	    {
		printf("<li>%d : ", id);
		rc = patch_delete(id);
		if (rc != 1)
		{
		    h_puts("Failed: ");
		    h_puts(db_error());
		}
		else
		    h_puts("Removed");
		puts("</li>");
	    }
	puts("</ul>");
	
	return 0;
    }
    
    h_header("Confirm remove patches");
    
    h_form_start("post", request_url);

    h_form_str("confirm", "hidden", 1, "yes");
    
    puts("<h3>Patches</h3>");
    
    h_table_start("patches");
    print_patch_header(1);

    pn = 0;
    for (i = 0; i < plist_length(idlist); i++)
	if (sscanf(plist_get(idlist, i), "%u", &id) == 1)
	{
	    PATCH pbuf;
	    
	    patch_init(&pbuf);
	    if (patch_lookup_id(&pbuf, id) == id)
		print_patch(&pbuf, NULL);
	    else
		++errors;
	}
    
    h_table_end();
    
    h_table_start("footer");
    h_tr_start("footer");
    h_td_start("selectL", 1);
    h_td_end();
    h_td_start("summary", 1);
    printf("%d patch%s found",
	   pn, pn != 1 ? "es" : "");
    if (maxlist && pn > maxlist)
	printf(" (%d shown)", maxlist);
    h_td_end();
    h_td_start("selectR", 1);
    h_puts(NULL);
    h_td_end();
    h_tr_end();
    h_table_end();

    puts("<h3>Action</h3>");
    h_button_submit("action", "remove-patches", TEXT_REMOVE_PATCHES);
    h_form_end();
    
    return 0;
}



int
endpoint_form_get(ENDPOINT *ep,
		  int row)
{
    int got_port = 0;
    int got_id = 0;
    int got_type = 0;
    int got_name = 0;
    int got_class = 0;
    int got_room = 0;
    int got_comment = 0;
    char *cp;
    
    
    form_set_row(row);
    endpoint_init(ep);

    got_id = form_geti("id", &ep->id);
    got_type = form_getu("type", &ep->type);
    got_name = form_gets("name", &ep->name);
    if (ep->name && (cp = strchr(ep->name, ':')) != NULL)
    {
	*cp++ = '\0';
	ep->port = strdup(cp);
    }

    got_port = form_gets("port", &ep->port);
    got_class = form_gets("class", &ep->class);
    got_room = form_gets("room", &ep->room);
    got_comment = form_gets("comment", &ep->comment);

    if (got_room)
	ep->room = fix_room(ep->room);
    
    if (got_type && got_name && got_class && got_room)
	return 1;

    return 0;
}


int
remove_ports(int argc,
	     char *argv[])
{
    int rc, i;
    unsigned int id = 0;
    unsigned int errors = 0;
    char *idstr = NULL;
    PLIST *idlist = NULL;
    char *confirm = NULL;

    
    if (form_gets("eid", &idstr) != 1 || !(idlist = plist_parse(NULL, idstr, " ,")))
    {
	h_header("Error removing ports");
	
	puts("Error: Need port id(s)");
	return 0;
    }
	    
    form_gets("confirm", &confirm);
    if (confirm && strcmp(confirm, "yes") == 0)
    {
	h_header("Removing ports");
	
	en = 0;
	puts("<ul>");
	for (i = 0; i < plist_length(idlist); i++)
	    if (sscanf(plist_get(idlist, i), "%u", &id) == 1)
	    {
		printf("<li>%d : ", id);
		rc = endpoint_delete(id);
		if (rc < 0)
		{
		    h_puts("Failed: ");
		    h_puts(db_error());
		}
		else if (rc == 0)
		    h_puts("Not removed");
		else
		    h_puts("Removed");
		puts("</li>");
	    }
	puts("</ul>");
	
	return 0;
    }

    h_header("Confirm remove ports");
    
    h_form_start("post", request_url);

    h_form_str("confirm", "hidden", 1, "yes");
    
    h_table_start("endpoints");
    print_port_header(1);

    pn = 0;
    for (i = 0; i < plist_length(idlist); i++)
	if (sscanf(plist_get(idlist, i), "%u", &id) == 1)
	{
	    ENDPOINT ebuf;
	    
	    endpoint_init(&ebuf);
	    if (endpoint_lookup_id(&ebuf, id) == id)
		print_port(&ebuf, NULL);
	    else
		++errors;
	}
    
    h_table_end();
    
    h_table_start("footer");
    h_tr_start("footer");
    
    h_td_start("selectL", 1);
    h_td_end();
    
    h_td_start("summary", 1);
    printf("%d port%s found",
	   en, en != 1 ? "s" : "");
    if (maxlist && en > maxlist)
	printf(" (%d shown)", maxlist);
    h_td_end();
    
    h_td_start("selectR", 1);
    h_puts(NULL);
    h_td_end();

    h_tr_end();
    h_table_end();

    puts("<h3>Action</h3>");
    h_button_submit("action", "remove-ports", TEXT_REMOVE_PORTS);
    h_form_end();
    
    return 0;
}



int
print_endpoint_add_form(ENDPOINT *ep)
{
    int errflag = 0;
    char nbuf[256];
    
    
    h_tr_start(NULL);

#if 0
    h_td_start("id", 1);
    fputs("&nbsp;", stdout);
    h_td_end();
#endif
    
    h_td_start("name", 1);
    h_form_str("name", "text", 40, endpoint2str(ep, nbuf, sizeof(nbuf), ep->room));
    h_td_end();
    
    h_td_start("room", 1);
    h_form_str("room", "text", 10, ep->room);
    h_td_end();
    
    h_td_start("type", 1);
    print_type_select("type", ep->type);
    h_td_end();
	
    h_td_start("class", 1);
    h_form_str("class", "text", 10, ep->class);
    h_td_end();
    
    h_td_start("comment", 1);
    h_form_str("comment", "text", 40, ep->comment);
    h_td_end();
    
    if (errflag)
    {
	h_td_start("state", 1);
	if (errflag)
	    fputs("<blink><font color=\"red\" size=\"+1\">*</font></blink>", stdout);
	h_td_end();
    }
    
    h_tr_end();

    if (errflag)
    {
	++g_errflag;
	return -errflag;
    }
    
    return 0;
}


int
add_port(int argc,
	 char *argv[])
{
    int rc;
    ENDPOINT ebuf;
    char *confirm = NULL;
    

    if (!acl_update)
    {
	h_header("Access denied");
	puts("Your user access level does not allow to to perform this operation.");
	return 0;
    }
    
    form_gets("confirm", &confirm);

    rc = endpoint_form_get(&ebuf, 0);
    if (rc > 0 && confirm && strcmp(confirm, "yes") == 0)
    {
	rc = endpoint_insert(&ebuf);
	if (rc != 1)
	{
	    h_header("Port add failed");
	    printf("Failure adding port");
	    if (ebuf.id)
		printf(" (id #%d)", ebuf.id);
	    printf(": ");
	    h_puts(db_error());
	}
	else
	{
	    char nbuf[256];


	    nbuf[0] = 0;
	    xprintf(nbuf, sizeof(nbuf), "%s", ebuf.name);
	    if (ebuf.port)
		xprintf(nbuf, sizeof(nbuf), ":%s", ebuf.port);

	    h_header("Port added");
	    h_form_start("post", request_url);
	    h_table_start("endpoints");
	    print_port_header(1);
	    endpoint_lookup(&ebuf, nbuf, ebuf.room);
	    print_port(&ebuf, NULL);
	    h_table_end();
	    
	    puts("<h3>Action</h3>");
	    h_button_submit("action", "update-ports",   TEXT_UPDATE_PORTS);
	    h_button_submit("action", "remove-ports",   TEXT_REMOVE_PORTS);
	}
    }
    else
    {
	h_header("Port add");
	
	g_errflag = 0;
	    
	h_form_start("post", request_url);
	h_table_start("endpoints");
	print_port_header(2);
	rc = print_endpoint_add_form(&ebuf);
	h_table_end();
	
	if (g_errflag)
	    puts("<p><i>Please note: Invalid data in one or more fields detected.</i>\n");

	h_form_str("confirm", "hidden", 0, "yes");
	
	puts("<h3>Action</h3>");
	h_button_submit("action", "add-port", TEXT_ADD_PORT);
	h_form_end();
    }
    
    return 0;
}


int
parse_config(const char *path)
{
    FILE *fp;
    char buf[1024];
    char *cp, *var, *val;


    fp = fopen(path, "r");
    if (!fp)
	return -1;

    while (fgets(buf, sizeof(buf), fp))
    {
	cp = strchr(buf, '#');
	if (cp)
	    *cp = '\0';

	for (cp = buf; *cp && isspace(*cp); ++cp)
	    ;

	var = strtok(cp, "=\n\r");
	if (!var || !*var)
	    continue;

	val = strtok(NULL, "\n\r");
	if (!val)
	    continue;
	
	if (strcmp(var, "dbuser") == 0)
	    db_user = strdup(val);
	
	else if (strcmp(var, "dbpass") == 0)
	    db_pass = strdup(val);
	
	else if (strcmp(var, "dbname") == 0)
	    db_name = strdup(val);
	
	else if (strcmp(var, "dbhost") == 0)
	    db_host = strdup(val);
	
	else if (strcmp(var, "admins") == 0)
	    admins = strdup(val);
	
    }

    fclose(fp);
    return 0;
}



int
main(int argc,
     char *argv[])
{
    char *cp;
    int i, rc;
    
    
    action = NULL;

    
    puts("Content-Type: text/html\n");

    time(&now);
    srand(now);
    
    setlocale(LC_ALL, "sv");
    
    cp = getenv("DBUSER");
    if (cp)
	db_user = strdup(cp);
    
    cp = getenv("DBPASS");
    if (cp)
	db_pass = strdup(cp);
    
    cp = getenv("ADMINS");
    if (cp)
	admins = strdup(cp);
    
    cp = getenv("CONFIG");
    if (cp)
	path_config = strdup(cp);
    
    parse_config(path_config);
    parse_config(".patches");

    remote_user = getenv("REDIRECT_REMOTE_USER");
    if (!remote_user)
	remote_user = getenv("REMOTE_USER");

    if (remote_user && is_admin_user(remote_user))
    {
	acl_update = 1;
    }
    
    
    remote_addr = getenv("REMOTE_ADDR");
    request_uri = getenv("REQUEST_URI");
    if (request_uri)
    {
	request_url = strdup(request_uri);
	cp = strchr(request_url, '?');
	if (cp)
	    *cp = '\0';
    }
    else
	request_uri = request_url = "?";
    
    if (!remote_addr)
	remote_addr = "unknown";

#if 1
    i = 1;
#else
    for (i = 1; i < argc && argv[i][0] == '-'; i++)
	switch (argv[i][1])
	{
	  case 'd':
	    ++debug;
	    break;
	    
	  case 'n':
	    ++nowrite;
	    break;
	    
	  case 't':
	    sscanf(argv[i]+2, "%d", &type);
	    break;
	    
	  case 'F':
	    path_config = strdup(argv[i]+2);
	    break;

	  case 'p':
	    pprefix = strdup(argv[i]+2);
	    break;

	  case 'f':
	    sscanf(argv[i]+2, "%d", &pfill);
	    break;
	    
	  case 'c':
	    class = strdup(argv[i]+2);
	    break;

	  case 'C':
	    comment = strdup(argv[i]+2);
	    break;

	  case 'R':
	    room = fix_room(argv[i]+2);
	    if (!room)
		fatal("Invalid room: %s", argv[i]+2);
	    break;

	  case 'N':
	    name = fix_port(argv[i]+2);
	    if (!name)
		fatal("Invalid port: %s", argv[i]+2);
	    break;

	  case 'U':
	    db_user = strdup(argv[i]+2);
	    break;

	  case 'P':
	    db_pass = strdup(argv[i]+2);
	    break;
	    
	  case 'D':
	    db_name = strdup(argv[i]+2);
	    break;
	    
	  case 'H':
	    db_host = strdup(argv[i]+2);
	    break;
	    
	  case 'A':
	    admins = strdup(argv[i]+2);
	    break;
	    
	  default:
	    fatal("Invalid command line option: -%c", argv[i][2]);
	}
#endif
    
    if (i < argc)
	action = strdup(argv[i]);

    rc = form_init(stdin);
    
    form_geti("debug", &debug);
    form_geti("nowrite", &nowrite);
    form_geti("verbose", &verbose);
    form_geti("max", &maxlist);
    
    if (db_init(db_user, db_pass, db_name, db_host) < 0)
	fatal("Failure connecting to database: %s", db_error());

    cp = NULL;
    if (form_gets("action", &cp) > 0)
	action = cp;
    
    form_gets("order", &order);

    form_getu("type", &type);
    
    form_gets("room", &room);
    if (room && *room)
	room = fix_room(room);
    
    form_gets("name", &name);
    if (name && *name)
	name = fix_port(name);
    
    form_gets("class", &class);
    form_gets("comment", &comment);

    if (action)
    {
	if (strcmp(action, TEXT_LIST_PATCHES) == 0)
	    action = "list-patches";

	else if (strcmp(action, TEXT_LIST_PORTS) == 0)
	    action = "list-ports";
	
	else if (strcmp(action, TEXT_ADD_PATCH) == 0)
	    action = "add-patch";
	
	else if (strcmp(action, TEXT_ADD_PORT) == 0)
	    action = "add-port";
	
	else if (strcmp(action, TEXT_LIST_OUTLETS) == 0)
	{
	    type = 1;
	    action = "list-ports";
	}
	
	else if (strcmp(action, TEXT_LIST_SPLITTERS) == 0)
	{
	    type = 2;
	    action = "list-ports";
	}
	
	else if (strcmp(action, TEXT_LIST_SWITCHES) == 0)
	{
	    type = 3;
	    action = "list-ports";
	}
	
	else if (strcmp(action, TEXT_UPDATE_PATCHES) == 0)
	    action = "update-patches";
	
	else if (strcmp(action, TEXT_REMOVE_PATCHES) == 0)
	    action = "remove-patches";
	
	else if (strcmp(action, TEXT_UPDATE_PORTS) == 0)
	    action = "update-ports";
	
	else if (strcmp(action, TEXT_REMOVE_PORTS) == 0)
	    action = "remove-ports";
	
    }
    
    if (!action || strcmp(action, "list-ports") == 0)
    {
	list_ports(argc-i, argv+i);
    }
     
    else if (strcmp(action, "update-ports") == 0)
    {
	update_ports(argc-i, argv+i);
    }
    
    else if (strcmp(action, "list-patches") == 0)
    {
	list_patches(argc-i, argv+i);
    }
    
    else if (strcmp(action, "update-patches") == 0)
    {
	update_patches(argc-i, argv+i);
    }
    
    else if (strcmp(action, "remove-patches") == 0)
    {
	remove_patches(argc-i, argv+i);
    }
    
    else if (strcmp(action, "add-patch") == 0)
    {
	add_patch(argc-i, argv+i);
    }
    
    else if (strcmp(action, "add-port") == 0)
    {
	add_port(argc-i, argv+i);
    }
    
    else if (strcmp(action, "remove-ports") == 0)
    {
	remove_ports(argc-i, argv+i);
    }
    
    else
    {
	h_header("Error");
	h_puts("Invalid action: ");
	h_puts(action);
    }
    
    h_footer("PNPDB %s - Database: %s@%s - %s",
	     version, db_name, db_host, db_stat());
    
    mysql_close(mysql);
    return 0;
}

