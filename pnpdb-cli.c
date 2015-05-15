/* 
** patchdb.c - Administer the patch database
*/

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <locale.h>
#include <netdb.h>
#include <mysql.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "plist.h"
#include "dbmisc.h"

#define DEFAULT_ORDER_ENDPOINTS "room,name,port+0"
#define DEFAULT_ORDER_PATCHES   "id"

extern char version[];

char *db_ro_user = "public";
char *db_ro_pass = NULL;
char *db_rw_user = NULL;
char *db_rw_pass = NULL;            /* SQL user password */

char *db_name = "patchdata";
char *db_host = "mysql";

char *order = NULL;

int   sel_type = 0;
char *sel_room = NULL;
char *sel_class = NULL;
char *sel_comment = NULL;

char *new_room = NULL;

int interactive = 0;
int debug = 0;
int verbose = 0;
int force = 0;
int write_f = 0; /* -1 = default on, except in interactive mode, 0 = off, 1 = on */

int n_patches = 0;
int n_eq = 0;
int n_endpoints = 0;

int n_patches_act = 0;
int n_endpoints_act = 0;

char *argv0 = NULL;

typedef struct cmd {
  int (*cmd)(int argc, char *argv[]);
  char *name;
  char *opts;
  char *help;
} CMD;

extern CMD cmdv[];


int
puts_indented(const char *s, int level) {
  int i, lc = '\n', rc = -1, nc = 0;


  while (*s) {
    if (lc == '\n') {
      for (i = 0; i < level; i++)
	rc = putchar(' ');
      nc = 0;
    }

    if (nc > 70 && *s == ' ') {
      rc = putchar(lc = '\n');
      ++s;
    }
    else
      rc = putchar(lc = *s++);
    ++nc;
  }

  if (lc != '\n')
    rc = putchar('\n');

  return rc;
}

int
relocate_ep(ENDPOINT *ep, void *xp) {
  static char *old = NULL;


  ++n_endpoints;
  if (!old || strcmp(old, ep->name) != 0) {
    ++n_eq;
    if (old)
      free(old);
    old = strdup(ep->name);
    if (verbose)
      printf("*** %s @ %s:\n", ep->name, ep->room);
  }

  if (!write_f) {
    if (verbose)
      printf("*** NOT RELOCATED ENDPOINT %u to %s\n", ep->id, new_room);
  } else {
    ++n_endpoints_act;
    endpoint_update_room(ep->id, new_room);
    if (verbose)
      printf("*** RELOCATED ENDPOINT %u to %s\n", ep->id, new_room);
  }

  return 0;
}
  


int
purge_ep(ENDPOINT *ep, void *xp) {
  PATCH pbuf;
  int rc;
  static char *old = NULL;


  ++n_endpoints;
  if (!old || strcmp(old, ep->name) != 0) {
    ++n_eq;
    if (old)
      free(old);
    old = strdup(ep->name);
    if (verbose)
      printf("*** %s @ %s:\n", ep->name, ep->room);
  }

  
  rc = patch_lookup_endpoint(&pbuf, ep->id);

  switch (rc) {
  case 0:
    break;

  case -4:
    fprintf(stderr, "%s: %s:%s @ %s Error: Inconsistent database: Multiple patches connected\n", 
	    argv0, ep->name ? ep->name : "?",  ep->port ? ep->port : "?", ep->room ? ep->room : "?");
    return 1;

  default:
    if (rc < 0) {
      fprintf(stderr, "%s: %s:%s @ %s: Internal Error #%d\n", 
	      argv0, ep->name ? ep->name : "?",  ep->port ? ep->port : "?", ep->room ? ep->room : "?", -rc);
      return 1;
    }

    ++n_patches;
    if (write_f) {
      patch_delete(pbuf.id);
      ++n_patches_act;

      if (verbose)
	printf("  Patch %u deleted\n", pbuf.id);
    } else {
      if (verbose)
	printf("  Patch %u NOT deleted\n", pbuf.id);
    }
  }
  
  return 0;
}



int
delete_ep(ENDPOINT *ep, void *xp) {
  PATCH pbuf;
  int rc;
  static char *old = NULL;

  ++n_endpoints;

  if (!old || strcmp(old, ep->name) != 0) {
    ++n_eq;
    if (old)
      free(old);
    old = strdup(ep->name);
    if (verbose)
      printf("*** %s @ %s:\n", ep->name, ep->room);
  }

  rc = patch_lookup_endpoint(&pbuf, ep->id);

  switch (rc) {
  case 0:
    break;

  case -4:
    fprintf(stderr, "%s: %s:%s @ %s Error: Inconsistent database: Multiple patches connected\n", 
	    argv0, ep->name ? ep->name : "?",  ep->port ? ep->port : "?", ep->room ? ep->room : "?");
    return 1;

  default:
    if (rc < 0) {
      fprintf(stderr, "%s: %s:%s @ %s: Internal Error #%d\n", 
	      argv0, ep->name ? ep->name : "?",  ep->port ? ep->port : "?", ep->room ? ep->room : "?", -rc);
      return 1;
    }

    if (force) {
      if (write_f) {
	patch_delete(pbuf.id);
	++n_patches_act;

	if (verbose)
	  printf("  Patch %u deleted\n", pbuf.id);
      } else {
	if (verbose)
	  printf("  Patch %u NOT deleted\n", pbuf.id);
      }
    }
  }
  
  if (write_f) {
    endpoint_delete(ep->id);
    ++n_endpoints_act;

    if (verbose)
      printf("  Endpoint %u (%s:%s @ %s) deleted\n", 
	     ep->id, ep->name ? ep->name : "?",  ep->port ? ep->port : "?", ep->room ? ep->room : "?");
  } else {
    if (verbose)
      printf("  Endpoint %u (%s:%s @ %s) NOT deleted\n", 
	     ep->id, ep->name ? ep->name : "?",  ep->port ? ep->port : "?", ep->room ? ep->room : "?");
  }

  return 0;
}



int
validate_empty_ep(ENDPOINT *ep, void *xp) {
  PATCH pbuf;
  int rc;

  
  ++n_endpoints;
  rc = patch_lookup_endpoint(&pbuf, ep->id);

  if (verbose > 1) {
    if (rc > 0)
      printf("  Endpoint %u (%s:%s @ %s) -> Patch %u\n", 
	     ep->id, ep->name ? ep->name : "?",  ep->port ? ep->port : "?", ep->room ? ep->room : "?", pbuf.id);
    else
      printf("  Endpoint %u (%s:%s @ %s)\n", 
	     ep->id, ep->name ? ep->name : "?",  ep->port ? ep->port : "?", ep->room ? ep->room : "?");
  }

  switch (rc) {
  case 0:
    break;

  case -4:
    n_patches = -1;
    fprintf(stderr, "%s: %u (%s:%s @ %s): Error: Inconsistent database: Multiple patches connected\n", 
	    argv0, ep->id, ep->name ? ep->name : "?",  ep->port ? ep->port : "?", ep->room ? ep->room : "?");
    return 1;
    break;

  default:
    if (rc < 0) {
      n_patches = -1;
      fprintf(stderr, "%s: %u (%s:%s @ %s): Internal Error #%d\n", 
	      argv0, ep->id, ep->name ? ep->name : "?",  ep->port ? ep->port : "?", ep->room ? ep->room : "?", -rc);
      return 1;
    }

    ++n_patches;
    if (verbose == 1) {
      if (n_patches == 1)
	puts("  Patches attached:");
      printf("    %u : %s:%-2s @ %s (%u)\n", pbuf.id, ep->name ? ep->name : "?",  ep->port ? ep->port : "?", ep->room ? ep->room : "?", ep->id);
    }
  }

  return 0;
}



int
print_ep(ENDPOINT *ep, void *xp) {
  PATCH pbuf;
  int rc;
  char *ts = endpoint_type2str(ep->type, 0);


  ++n_endpoints;
  printf("%-5u  %-4s  %-10s  %-4s  %-5s  %-10s  %-30s  ",
	 ep->id,
	 ts ? ts : "?",
	 ep->name ? ep->name : "?",
	 ep->port ? ep->port : "?",
	 ep->class ? ep->class: "?",
	 ep->room ? ep->room : "?",
	 ep->comment ? ep->comment : "?");

  rc = patch_lookup_endpoint(&pbuf, ep->id);

  switch (rc) {
  case 0:
    break;

  case -4:
    printf("** Error: Multiple patches **");
    break;

  default:
    if (rc < 0)
      printf("** Error: %d **", rc);
    else
      printf("%u", pbuf.id);
  }

  putchar('\n');

  return 0;
}


char *
e2str(ENDPOINT *ep) {
  char buf[1024];

  if (ep->port && ep->port[0])
    sprintf(buf, "%s:%s @ %s", ep->name, ep->port ? ep->port : "?", ep->room ? ep->room : "- Undefined -");
  else
    sprintf(buf, "%s @ %s", ep->name, ep->room ? ep->room : "- Undefined -");
  return strdup(buf);
}


int
print_list_ep(ENDPOINT *ep, void *xp) {
  static char *old = NULL;


  ++n_endpoints;

  if (verbose || !old || strcmp(old, ep->name) != 0) {
    ++n_eq;

    if (verbose)
      puts(e2str(ep));
    else
      printf("%s @ %s\n", ep->name, ep->room);

    if (old)
      free(old);
    old = strdup(ep->name);
  }

  return 0;
}


int
fsck_patch(PATCH *pp, void *xp) {
  ENDPOINT eb1, eb2;
  char *es1, *es2;


  ++n_patches;
  es1 = es2 = NULL;

  if (endpoint_lookup_id(&eb1, pp->p1) > 0)
    es1 = e2str(&eb1);
  
  if (endpoint_lookup_id(&eb2, pp->p2) > 0)
    es2 = e2str(&eb2);
  
  if (pp->p1 == 0 || pp->p2 == 0) {
    ++n_patches_act;
    printf("  %5u: Dangling endpoint: %u (%s) - %u (%s)\n", pp->id, pp->p1, es1 ? es1 : "?", pp->p2, es2 ? es2 : "?");
  }
  
  if (pp->p1 && !es1) {
    ++n_patches_act;
    printf("  %5u: Missing endpoint: %u\n", pp->id, pp->p1);
  }

  if (pp->p2 && !es2) {
    ++n_patches_act;
    printf("  %5u: Missing endpoint: %u\n", pp->id, pp->p2);
  }
  
  return 0;
}


int
fsck_ep(ENDPOINT *ep, void *xp) {
  static char *old_name = NULL;
  static char *old_port = NULL;
  static char *old_room = NULL;
  static int old_type = -1;
  static int nm = 0;


  ++n_endpoints;

  if (!ep->room) {
    ++n_endpoints_act;
    printf("  %s (%u): No room defined\n", e2str(ep), ep->id);
  }
  
  if (verbose && ep->room && !valid_room(ep->room)) {
    ++n_endpoints_act;
    printf("  %s (%u): Invalid room\n", e2str(ep), ep->id);
  }
  

  if (old_name && strcmp(old_name, ep->name) == 0 && ((!old_port & !ep->port) || (old_port && strcmp(old_name, ep->port) == 0))) {
    ++nm;
    if (old_type == 3 && nm > 1)
      printf("  %s: Has more that 1 equipment endpoints\n", e2str(ep));
    else if (old_type == 1 && nm > 2)
      printf("  %s: Has more that 2 outlet endpoints\n", e2str(ep));
  } else {
    nm=0;
    if (old_name)
      free(old_name);
    old_name = ep->name ? strdup(ep->name) : NULL;
    if (old_port)
      free(old_port);
    old_port = ep->port ? strdup(ep->port): NULL;
    if (old_room)
      free(old_room);
    old_room = ep->room ? strdup(ep->room) : NULL;
    old_type = ep->type;
  }

  return 0;
}






int
cmd_list_equipment(int argc,
		   char *argv[]) {
  int i;


  if (verbose || interactive) {
    puts("Equipment");
    puts("----------------------");
  }
  
  if (argc == 1) { 
    n_endpoints = 0;
    endpoint_foreach(print_list_ep, NULL, EP_TYPE_EQUIPMENT, NULL, sel_room, sel_class, sel_comment, NULL, "room,name");
    if (n_endpoints == 0) {
      fprintf(stderr, "%s: No endpoints found\n", argv0);
      return 1;
    }
  } else {
    for (i = 1; i < argc; i++) {
      n_endpoints = 0;
      endpoint_foreach(print_list_ep, NULL, sel_type, argv[i], sel_room, sel_class, sel_comment, NULL, "room,name");
      if (n_endpoints == 0) {
	fprintf(stderr, "%s: No endpoints found\n", argv0);
	return 1;
      }
    }
  }

  return 0;
}


int
cmd_check_database(int argc,
		  char *argv[]) {
  const char *sp;


  n_patches = 0;
  n_endpoints = 0;
  n_patches_act = 0;
  n_endpoints_act = 0;


  sel_type = 0;
  
  puts("Checking endpoints:");

  endpoint_foreach(fsck_ep, NULL, sel_type, NULL, sel_room, sel_class, sel_comment, NULL, NULL);
  if (n_endpoints == 0) {
    fprintf(stderr, "%s: No endpoints found\n", argv0);
    return 1;
  }

  printf("%u of %u endpoints with problems.\n", n_endpoints_act, n_endpoints);


  puts("\nChecking patches:");

  patch_foreach(fsck_patch, NULL, sel_type, NULL, sel_room, sel_class, sel_comment, NULL, NULL);
  if (n_patches == 0) {
    fprintf(stderr, "%s: No patches found\n", argv0);
    return 1;
  }
  
  printf("%u of %u patches with problems.\n", n_patches_act, n_patches);

  printf("\nMySQL server status:\n  ");
  sp = db_stat();
  while (*sp) {
    if (sp[0] == ' ' && sp[1] == ' ')
      putchar('\n');
    putchar(*sp++);
  }
  putchar('\n');
    
  return 0;
}


int
print_patch(PATCH *pp, void *xp) {
  ENDPOINT eb1, eb2;
  char *es1, *es2, b1[10], b2[10];
 

  if (endpoint_lookup_id(&eb1, pp->p1) > 0)
    es1 = endpoint2str(&eb1, NULL, 0, NULL);
  else {
    if (pp->p1 != 0) {
      sprintf(b1, "#%u", pp->p1);
      es1 = b1;
    } else
      es1 = "-Not Connected-";
  }
  
  if (endpoint_lookup_id(&eb2, pp->p2) > 0)
    es2 = endpoint2str(&eb2, NULL, 0, NULL);
  else {
    if (pp->p2 != 0) {
      sprintf(b2, "#%u", pp->p2);
      es2 = b2;
    } else
      es2 = "-Not Connected-";
  }
  
  printf("%-5u  %-20s  %-20s  %-20s  %s\n",
	 pp->id,
	 es1, es2,
	 pp->comment ? pp->comment : "",
	 pp->date ? pp->date : "");

  n_patches++;

  return 0;
}


int
cmd_show_patches(int argc,
		    char *argv[]) {
  int rc = 0;
  

  if (verbose || interactive) {
    printf("%-5s  %-20s  %-20s  %-20s  %s\n",
	   "#",
	   "Endpoint 1",
	   "Endpoint 2",
	   "Comment",
	   "Last updated");
    printf("%-5s  %-20s  %-20s  %-20s  %s\n",
	   "-----",
	   "--------------------",
	   "--------------------",
	   "--------------------",
	   "---------------");
  }

  n_patches = 0;
  rc = patch_foreach(print_patch, NULL, sel_type, argv[1], sel_room, sel_class, sel_comment, NULL, order ? order : "id");

  if (n_patches == 0)
    puts("- No patches found -");
#if 0
  printf("%d patch%s found.\n", n_patches, n_patches == 1 ? "" : "es");
#endif

  return rc;
}


int
cmd_print_endpoints(int argc,
		    char *argv[]) {
  int i, rc = 0;
  

  if (verbose || interactive) {
    printf("%-5s  %-4s  %-10s  %-4s  %-5s  %-10s  %-30s  %s\n",
	   "#",
	   "Type",
	   "Name",
	   "Port",
	   "Class",
	   "Room",
	   "Comment",
	   "Patch #");
    
    printf("%-5s  %-4s  %-10s  %-4s  %-5s  %-10s  %-30s  %s\n",
	   "-----",
	   "----",
	   "----------",
	   "----",
	   "-----",
	   "----------",
	   "------------------------------",
	   "----------");
  }
  
  if (argc > 1) {
    for (i = 1; i < argc; i++) {
      n_endpoints = 0;
      rc = endpoint_foreach(print_ep, NULL, sel_type, argv[i], sel_room, sel_class, sel_comment, 
			    NULL, order ? order : DEFAULT_ORDER_ENDPOINTS);
      if (n_endpoints == 0) {
	fprintf(stderr, "%s: %s: Invalid name\n", argv0, argv[i]);
	return 1;
      }
    }
  } else {
    n_endpoints = 0;
    rc = endpoint_foreach(print_ep, NULL, sel_type, NULL, sel_room, sel_class, sel_comment, 
			  NULL, order ? order : DEFAULT_ORDER_ENDPOINTS);
    if (n_endpoints == 0) {
      fprintf(stderr, "%s: No endpoints found\n", argv0);
      return 1;
    }
  }

  return rc;
}


int
cmd_purge_endpoints(int argc,
		   char *argv[]) {
  int i, rc;


  if (argc <= 1) {
    fprintf(stderr, "%s: Missing required endpoint name(s) for purge action\n", argv0);
    return 1;
  }

  for (i = 1; i < argc; i++) {
    n_endpoints = 0;
    n_patches_act = 0;
    rc = endpoint_foreach(purge_ep, NULL, sel_type, argv[i], sel_room, sel_class, sel_comment, 
			  NULL, order ? order : DEFAULT_ORDER_ENDPOINTS);
    if (rc != 0)
      return 1;
    
    if (n_endpoints == 0) {
      fprintf(stderr, "%s: %s: Invalid name\n", argv0, argv[i]);
      return 1;
    }
    
    if (!verbose) {
      if (!write_f)
	printf("%s: %d patch(es) NOT deleted.\n", argv[i], n_patches_act);
      else
	printf("%s: %d patch%s deleted.\n", 
	       argv[i], n_patches_act, (n_patches_act == 1) ? "" : "es");
    }
  }

  return 0;
}


int
cmd_relocate_endpoints(int argc,
		      char *argv[]) {
  int i, rc = 0;


  if (argc <= 1) {
    fprintf(stderr, "%s: Missing required new room for relocate action\n", argv0);
    return 1;
  }

  new_room = fix_room(argv[1]);

  if (!valid_room(new_room)) {
    fprintf(stderr, "%s: %s: Invalid room\n", argv0, argv[1]);
    return 1;
  }

  if (argc <= 2) {
    fprintf(stderr, "%s: Missing required endpoint name(s) for relocate action\n", argv0);
    return 1;
  }
  
  for (i = 2; i < argc; i++) {
    n_patches = 0;
    n_endpoints = 0;
    n_endpoints_act = 0;
    
    rc = endpoint_foreach(validate_empty_ep, NULL, sel_type, argv[i], sel_room, sel_class, sel_comment, 
			  NULL, order ? order : DEFAULT_ORDER_ENDPOINTS);
    if (n_patches < 0 || rc != 0)
      return 1;
    
    if (n_endpoints == 0) {
      fprintf(stderr, "%s: %s: Invalid name\n", argv0, argv[i]);
      return 1;
    }
    
    if (n_patches > 0) {
      if (!force) {
	fprintf(stderr, "%s: %s: %d attached patch%s found. Relocation aborted.\n", 
		argv0, argv[i], n_patches, (n_patches == 1) ? "" : "es");
	return 1;
      }
    }
    
    n_patches = 0;
    n_endpoints = 0;
    n_endpoints_act = 0;

    rc = endpoint_foreach(relocate_ep, NULL, sel_type, argv[i], sel_room, sel_class, sel_comment, NULL, order ? order : DEFAULT_ORDER_ENDPOINTS);
    if (rc != 0)
      return 1;
    
    if (!verbose) {
      if (!write_f)
	printf("%s: %d endpoint%s NOT relocated.\n", 
	       argv[i], n_endpoints, (n_endpoints == 1) ? "" : "s");
      else
	printf("%s: %d endpoint%s relocated.\n", 
	       argv[i], n_endpoints_act, (n_endpoints_act == 1) ? "" : "s");
    }
  }

  return 0;
}

int
cmd_delete_endpoints(int argc,
		    char *argv[]) {
  int i, rc;


  if (argc <= 1) {
    fprintf(stderr, "%s: Missing required endpoint name(s) for delete action\n", argv0);
    return 1;
  }
  
  for (i = 1; i < argc; i++) {
    n_patches = 0;
    n_endpoints = 0;
    n_patches_act = 0;
    n_endpoints_act = 0;
    
    rc = endpoint_foreach(validate_empty_ep, NULL, sel_type, argv[i], sel_room, sel_class, sel_comment, NULL, order ? order : DEFAULT_ORDER_ENDPOINTS);
    if (n_patches < 0 || rc != 0)
      return 1;
    
    if (n_endpoints == 0) {
      fprintf(stderr, "%s: %s: Invalid name\n", argv0, argv[i]);
      return 1;
    }
    
    if (n_patches > 0) {
      if (!force) {
	fprintf(stderr, "%s: %s: %d attached patch%s found. Deletion aborted.\n", 
		argv0, argv[i], n_patches, (n_patches == 1) ? "" : "es");
	return 1;
      }
    }

    n_patches = 0;
    n_endpoints = 0;
    n_patches_act = 0;
    n_endpoints_act = 0;
    
    rc = endpoint_foreach(delete_ep, NULL, sel_type, argv[i], sel_room, sel_class, sel_comment, NULL, order ? order : DEFAULT_ORDER_ENDPOINTS);
    if (rc != 0)
      return 1;
    
    if (!verbose) {
      if (n_patches == 0) {
	if (!write_f)
	  printf("%s: %d endpoint%s NOT deleted.\n", 
		 argv[i], n_endpoints, (n_endpoints_act == 1) ? "" : "s");
	else
	  printf("%s: %d endpoint%s deleted.\n", 
		 argv[i], 
		 n_endpoints_act, (n_endpoints_act == 1) ? "" : "s");
      } else {
	if (!write_f)
	  printf("%s: %d endpoint%s and %d patch%s NOT deleted.\n", 
		 argv[i], 
		 n_endpoints, (n_endpoints == 1) ? "" : "s",
		 n_patches, (n_patches == 1) ? "" : "es");
	else
	  printf("%s: %d endpoint%s and %d patch%s deleted.\n", 
		 argv[i], 
		 n_endpoints_act, (n_endpoints_act == 1) ? "" : "s",
		 n_patches_act, (n_patches_act == 1) ? "" : "es");
      }
    }
  }

  return 0;
}




int
cmd_create_endpoints(int argc,
		    char *argv[]) {
  int i, j, rc = 0;
  ENDPOINT ebuf;
  int first_port = -1, last_port = -1;
  char buf[64];


  if (argc < 6) {
    fprintf(stderr, "%s: Missing required arguments for create action\n", argv0);
    return 1;
  }

  endpoint_init(&ebuf);

  ebuf.name = strdup(argv[1]);

  ebuf.type = endpoint_str2type(argv[2]);
  if (ebuf.type <= 0) {
    fprintf(stderr, "%s: %s: Invalid type for create action\n", argv0, argv[2]);
    return 1;
  }
    
  ebuf.room = fix_room(argv[3]);
  if (!valid_room(ebuf.room)) {
    fprintf(stderr, "%s: %s: Invalid room for create action\n", argv0, argv[3]);
    return 1;
  }

  ebuf.comment = strdup(argv[4]);
  ebuf.class = strdup(argv[5]);

  n_endpoints_act = 0;

  for (i = 6; i < argc; i++) {
    if (debug)
      fprintf(stderr, "** CREATE PORTS: %s\n",argv[i]);

    if (sscanf(argv[i], "%u-%u", &first_port, &last_port) == 2) {
      for (j = first_port; j <= last_port; j++) {
	sprintf(buf, "%u", j);
	ebuf.port = buf;

	++n_endpoints_act;
	if (write_f) {
	  rc=(endpoint_insert(&ebuf) == 1 ? 0 : 1);
	  if (rc != 0) {
	    fprintf(stderr, "%s: %s: Endpoint creation failed.\n", argv0, endpoint2str(&ebuf, NULL, 0, NULL));
	    return rc;
	  }
	    
	  if (verbose)
	    printf("%s: Created\n", endpoint2str(&ebuf, NULL, 0, NULL));
	} else {
	  if (verbose)
	    printf("%s: NOT Created\n", endpoint2str(&ebuf, NULL, 0, NULL));
	}
      }
    } else {
      ebuf.port = strdup(argv[i]);

      ++n_endpoints_act;
      if (write_f) {
	rc=(endpoint_insert(&ebuf) == 1 ? 0 : 1);
	if (rc != 0) {
	  fprintf(stderr, "%s: %s: Endpoint creation failed.\n", argv0, endpoint2str(&ebuf, NULL, 0, NULL));
	  return rc;
	}
	
	if (verbose)
	  printf("%s: Created\n", endpoint2str(&ebuf, NULL, 0, NULL));
      } else {
	if (verbose)
	  printf("%s: NOT Created\n", endpoint2str(&ebuf, NULL, 0, NULL));
      }
    }
  }

  if (!verbose) {
    if (!write_f) {
      printf("%d endpoint%s NOT created\n",
	     n_endpoints_act, (n_endpoints_act == 1) ? "" : "s");
    } else {
      printf("%d endpoint%s created\n",
	     n_endpoints_act, (n_endpoints_act == 1) ? "" : "s");
    }
  }

  return rc;
}


int
cmd_lookup(const char *name, CMD **cp) {
  int i, nm;


  nm = 0;
  for (i = 0; cmdv[i].name; i++)
    if (strncmp(name, cmdv[i].name, strlen(name)) == 0) {
      ++nm;
      *cp = &cmdv[i];
    }
  
  return nm;
}

int
cmd_help(int argc,
	 char *argv[]) {
  int i, rc;
  CMD *cmdp = NULL;


  if (argc == 1) {
    printf("Actions (may be abbreviated and prefixed with variable assignments):\n");
    for (i = 0; cmdv[i].cmd; i++) 
      printf("  %s %s\n", cmdv[i].name, cmdv[i].opts);
    puts("Variables may be permanently set with with 'set' or directly on the");
    puts("command line. If a variable assignment is combined with an action then");
    puts("the assignment is only temporary during the exection of the action.");
    puts("Examples:");
    puts("  list-equipment");
    puts("  create sw11483 eq \"F F203\" \"Test Example\" TP 1-24 A1 A2");
    puts("  room=\"02270131\" type=outlet print");
  } else
    for (i = 1; i < argc; i++) {
      rc = cmd_lookup(argv[i], &cmdp);
      if (rc < 1) {
	fprintf(stderr, "%s: %s: Unable to describe action - Unknown action\n", argv0, argv[i]);
	return 1;
      }
      if (rc > 1) {
	fprintf(stderr, "%s: %s: Unable to describe action - Multiple matches\n", argv0, argv[i]);
	return 1;
      }

      if (i > 1)
	putchar('\n');
      puts("Usage:");
      printf("  %s %s\n", cmdp->name, cmdp->opts);
      if (cmdp->help) {
	puts("Description:");
	puts_indented(cmdp->help, 2);
      }
    }
    
  return 0;
}


void
usage(void) {
  char *ept = endpoint_type2str(sel_type, 1);

  
  if (!ept)
    ept = "*";

  printf("Usage:\n");
  printf("  %s [<options>] <action> [<arguments>]\n", argv0);
  printf("\nOptions:\n");
  printf("  -h           Print this information\n");
  printf("  -v           Increase verbosity\n");
  printf("  -n           Disable database update mode\n");
  printf("  -w           Enable database update mode\n");
  printf("  -f           Force write/update\n");
  printf("  -U <user>    Set database user (default: login name)\n");
  printf("  -P <pass>    Set database password (default: prompt)\n");
  printf("  -t <type>    Select object type (default: %s)\n", ept);
  printf("  -c <class>   Select class\n");
  printf("  -R <room>    Select location\n");
  printf("  -C <comment> Select comment\n");
  printf("  -O <order>   Override sort order");

  putchar('\n');
  cmd_help(0, NULL);
}


char *
str2qstr(const char *s) {
  char buf[1024];

  if (!s)
    return "<not set>";
  
  strcpy(buf, "\"");
  strcat(buf, s);
  strcat(buf, "\"");
  return strdup(buf);
}

int
cmd_get(int argc,
	 char *argv[]) {
  int i;
  char *ts = endpoint_type2str(sel_type, 2);

  
  if (argc > 1) 
    for (i = 1; i < argc; i++)
      if (strcasecmp(argv[i], "room") == 0)
	printf("Room = %s\n", str2qstr(sel_room));
      else if (strcasecmp(argv[i], "class") == 0)
	printf("Class = %s\n", str2qstr(sel_class));
      else if (strcasecmp(argv[i], "comment") == 0)
	printf("Comment = %s\n", str2qstr(sel_comment));
      else if (strcasecmp(argv[i], "type") == 0)
	printf("Type = %s\n", ts ? ts : "All");
      else if (strcasecmp(argv[i], "order") == 0)
	printf("Order = %s\n", str2qstr(order));
      else if (strcasecmp(argv[i], "Force") == 0)
	printf("Force = %s\n", force ? "Yes" : "No");
      else if (strcasecmp(argv[i], "verbose") == 0)
	printf("Verbose = %s\n", verbose ? "Yes" : "No");
      else {
	fprintf(stderr, "%s: %s: Unknown variable\n", argv0, argv[i]);
	return 1;
      }
  else {
    printf("Room = %s\n", str2qstr(sel_room));
    printf("Class = %s\n", str2qstr(sel_class));
    printf("Comment = %s\n", str2qstr(sel_comment));
    printf("Type = %s\n", ts ? ts : "All");
    printf("Order = %s\n", str2qstr(order));
    printf("Force = %s\n", force ? "Yes" : "No");
    printf("Verbose = %s\n", verbose ? "Yes" : "No");
  }
      
  return 0;
}

int
cmd_unset(int argc,
	char *argv[]) {
  int i;


  for (i = 1; i < argc; i++) {
    if (strcasecmp(argv[i], "room") == 0)
      sel_room = NULL;
    else if (strcasecmp(argv[i], "class") == 0)
      sel_class = NULL;
    else if (strcasecmp(argv[i], "comment") == 0)
      sel_comment = NULL;
    else if (strcasecmp(argv[i], "type") == 0)
      sel_type = 0;
    else if (strcasecmp(argv[i], "order") == 0)
      order = NULL;
    else if (strcasecmp(argv[i], "Force") == 0)
      force = 0;
    else if (strcasecmp(argv[i], "verbose") == 0)
      verbose = 0;
    else {
      fprintf(stderr, "%s: %s: Unknown variable\n", argv0, argv[i]);
      return 1;
    }
  }
  return 0;
}

int
cmd_set(int argc,
	char *argv[]) {
  int i;


  for (i = 1; i < argc; i++) {
    char *val = strchr(argv[i], '=');
    
    if (val)
      *val++ = '\0';
    else
      val = argv[2];
    
    if (strcasecmp(argv[i], "room") == 0) {
      if (!val) {
	fprintf(stderr, "%s: Missing required room value\n", argv0);
	return 1;
      }
      sel_room = strdup(val);
    }
    else if (strcasecmp(argv[i], "class") == 0) {
      if (!val) {
	fprintf(stderr, "%s: Missing required class value\n", argv0);
	return 1;
      }
      sel_class = strdup(val);
    }
    else if (strcasecmp(argv[i], "comment") == 0) {
      if (!val) {
	fprintf(stderr, "%s: Missing required comment value\n", argv0);
	return 1;
      }
      sel_comment = strdup(val);
    }
    else if (strcasecmp(argv[i], "type") == 0) {
      if (!val) {
	fprintf(stderr, "%s: Missing required type value\n", argv0);
	return 1;
      }
      sel_type = endpoint_str2type(val);
    }
    else if (strcasecmp(argv[i], "order") == 0) {
      if (!val) {
	fprintf(stderr, "%s: Missing required order value\n", argv0);
	return 1;
      }
      order = strdup(val);
    }
    else if (strcasecmp(argv[i], "force") == 0) {
      if (val) {
	if (sscanf(val, "%u", &force) != 1) {
	  fprintf(stderr, "%s: %s: Invalid force value\n", argv0, val);
	  return 1;
	}
      }
      else
	force = 1;
    }
    else if (strcasecmp(argv[i], "verbose") == 0) {
      if (val) {
	if (sscanf(val, "%u", &verbose) != 1) {
	  fprintf(stderr, "%s: %s: Invalid verbose value\n", argv0, val);
	  return 1;
	}
      }
      else
	verbose = 1;
    }
    else {
      fprintf(stderr, "%s: %s: Unknown variable\n", argv0, argv[i]);
      return 1;
    }
  }

  return 0;
}

int
cmd_quit(int argc,
	 char *argv[]) {
  if (interactive)
    puts("*** Terminating ***");
  return -1;
}

int
cmd_enable_write(int argc,
		 char *argv[]) {
  /* XXX: Check if database allows UPDATE */

  if (write_f)
    return 0;

  if (!db_rw_pass)
    db_rw_pass = getpassphrase("Enter password: ");

  db_close();

  if (db_init(db_rw_user, db_rw_pass, db_name, db_host) < 0) {
    fprintf(stderr, "%s: db_init(): %s\n", argv[0], db_error());

    if (db_init(db_ro_user, db_ro_pass, db_name, db_host) < 0) {
      fprintf(stderr, "%s: FATAL ERRROR: Unable to reopen database read-only: %s\n", argv[0], db_error());
    }
    return 1;
  }

  write_f = 1;
  return 0;
}

int
cmd_disable_write(int argc,
		  char *argv[]) {
  if (!write_f)
    return 0;

  db_close();

  if (db_init(db_ro_user, db_ro_pass, db_name, db_host) < 0) {
    fprintf(stderr, "%s: FATAL ERRROR: Unable to reopen database read-only: %s\n", argv[0], db_error());
  }

  write_f = 0;
  return 0;
}


CMD cmdv[] = {
  { 
    cmd_help, 
    "help", 
    "[<action-1> .. <action-N>]",
    "Displays list of available actions. With argument, display more information about a specific action."
  },

  { 
    cmd_enable_write, 
    "enable-write-mode", 
    "",
    "Enable database write mode"
  },

  { 
    cmd_disable_write, 
    "disable-write-mode", 
    "",
    "Disable database write mode"
  },

  { 
    cmd_list_equipment, 
    "list-equipment",
    "[<name-1> .. <name-N>]",
    "List summary information about equipment."
  },

  { 
    cmd_print_endpoints, 
    "print-endpoints",
    "[<name-1> .. <name-N>]",
    "Print detailed information about endpoints."
  },

  { 
    cmd_purge_endpoints, 
    "purge-endpoints",
    "<name-1> [.. <name-N>]",
    "Purge endpoints - removes installed patches."
  },

  { 
    cmd_delete_endpoints,
    "delete-endpoints",
    "<name-1> [.. <name-N>]",
    "Deletes endpoints. Requires that they are empty (does not have patches installed)."
  },

  { 
    cmd_relocate_endpoints, 
    "relocate-endpoints", 
    "<new-room> <name-1> [.. <name-N>]",
    "Relocates endpoints to another location. For example used when moving equipment."
  },

  { 
    cmd_create_endpoints,
    "create-endpoints",
    "<name> <type> <room> <comment> <class> <ports> [ .. <ports>]",
    "Create new endpoints. Example:\n  create sw19999 EQ \"F F203\" \"Test Example\" TP 1-24 A1 A2"
  },

  { 
    cmd_check_database, 
    "check-database",
    "",
    "Check consistency and health of database."
  },

  { 
    cmd_get,
    "get-variable",
    "[<name-1> .. <name-N>]",
    "Get and show the contents of variables (all if none selected)."
  },

  { 
    cmd_set,
    "set-variable",
    "<name>=<value> [.. <name>=<value>]",
    "Set the named variable(s)."
  },

  { 
    cmd_unset,
    "unset-variable",
    "<name-1> [.. <name-N>]",
    "Unset (clear) the named variable(s)."
  },

  { 
    cmd_show_patches,
    "show-patches",
    "<patch-1> [ .. <patch-N>]",
    "Show the patches"
  },

  { 
    cmd_quit,
    "quit",               
    "",
    "Quit from this program."
  },

  { NULL, NULL, NULL, NULL }
};

/*
  insert-patch
  remove-patch
 */




int
cmd_execute(int argc, 
	    char *argv[]) {
  int nm;
  CMD *cmdp = NULL;
  int i, rc = -1;
  char *targv[3];
  char *tmp_room, *tmp_class, *tmp_comment, *tmp_order;
  int tmp_type, tmp_force, tmp_verbose;


  targv[0] = "set";
  targv[2] = NULL;
  
  tmp_room = sel_room;
  tmp_class = sel_class;
  tmp_comment = sel_comment;
  tmp_type = sel_type;
  tmp_order = order;
  tmp_force = force;
  tmp_verbose = verbose;

  for (i = 0; i < argc && strchr(argv[i], '=') != NULL; i++) {
    targv[1] = argv[i];
    cmd_set(2, &targv[0]);
  }

  argc -= i;
  argv += i;

  if (!argc || !argv[0])
    return 0;

  if (strcmp(argv[0], "?") == 0) {
    argv[0] = "help";
  }

  nm = cmd_lookup(argv[0], &cmdp);
  if (nm < 1) {
    fprintf(stderr, "%s: %s: Unknown action\n", argv0, argv[0]);
    rc = 1;
    goto End;
  }
  
  if (nm > 1) {
    fprintf(stderr, "%s: %s: Non-unique action\n", argv0, argv[0]);
    rc = 1;
    goto End;
  }

  if (argc > 1 && strcmp(argv[1], "?") == 0) {
    argv[1] = argv[0];
    argv[0] = "help";
    argv[2] = NULL;
    rc = cmd_help(2, argv);
  }
  else 
    rc = cmdp->cmd(argc, argv);

 End:
  if (i > 0) {
    sel_room = tmp_room;
    sel_class = tmp_class;
    sel_comment = tmp_comment;
    sel_type = tmp_type;
    order = tmp_order;
    force = tmp_force;
    verbose = tmp_verbose;
  }

  return rc;
}



typedef struct sbuf {
  char *buf;
  int size;
  int len;
} SBUF;

int
sbuf_init(SBUF *sp) {
  sp->buf = "";
  sp->size = 0;
  sp->len = 0;
  return 0;
}

int
sbuf_destroy(SBUF *sp) {
  if (sp->buf)
    free(sp->buf);

  sp->size = 0;
  sp->len = 0;
  return 0;
}


#define SBUF_DEFAULT_SIZE 256

int
sbuf_putc(SBUF *sp, int c) {
  char *nbuf;


  if (sp->len >= sp->size-1) {
    if (sp->size > 0)
      nbuf = realloc(sp->buf, sp->size + SBUF_DEFAULT_SIZE);
    else
      nbuf = malloc(SBUF_DEFAULT_SIZE);
    if (!nbuf)
      return -1;

    sp->buf = nbuf;
    sp->size += SBUF_DEFAULT_SIZE;
  }

  sp->buf[sp->len++] = c;
  sp->buf[sp->len] = '\0';

  return c;
}

char *
sbuf_gets(SBUF *sp) {
  return sp->buf;
}


char *
argtok(char **buf) {
  SBUF sb;
  int delim = 0;


  if (!buf || !*buf)
    return NULL;

  sbuf_init(&sb);

  /* Skip leading whitespace */
  while (**buf && isspace(**buf))
    ++*buf;

  if (!**buf || **buf == ';')
    return NULL;

  while (**buf && (delim || !(isspace(**buf) || **buf == ';'))) {
    if (delim) {
      if (**buf == delim) {
	delim = 0;
      } else {
	sbuf_putc(&sb, **buf);
      }
      ++*buf;
    } else {
      if (**buf == '"' || **buf == '\'') {
	/* Got first " / ' */
	delim = **buf;
	++*buf;
      } else if (**buf == '\\' && ((*buf)[1] != '\0')) {
	++*buf;
	sbuf_putc(&sb, **buf);
	++*buf;
#if 0
      } else if ((*buf)[0] == '$') {
#endif
      } else {
	if (!isspace(**buf) && **buf != ';') {
	  sbuf_putc(&sb, **buf);
	  ++*buf;
	} else {
	  while (**buf && isspace(**buf))
	    ++*buf;
	}
      }
    }
  }

  return sbuf_gets(&sb);
}


#define MAXARGV 256

int
cli_execute(char *buf) {

  int argc = 0;
  char *argv[MAXARGV];


  while (argc < MAXARGV-1 && (argv[argc] = argtok(&buf)) != NULL)
    ++argc;

  if (!argc)
    return 0;

  if (argc >= MAXARGV) {
    fprintf(stderr, "%s: Input line too long\n", argv0);
    return -1;
  }

  return cmd_execute(argc, argv);
}




/* Generator function for command completion.  STATE lets us know whether
   to start from scratch; without any state (i.e. STATE == 0), then we
   start at the top of the list. */
char *
command_generator(const char *text, 
		  int state)
{
  static int list_index, len;
  char *name;
  
  
  /* If this is a new word to complete, initialize now.  This includes
     saving the length of TEXT for efficiency, and initializing the index
     variable to 0. */
  if (!state)
    {
      list_index = 0;
      len = strlen(text);
    }

  /* Return the next name which partially matches from the command list. */
  while ((name = cmdv[list_index].name) != NULL) {
    list_index++;
    
    if (strncmp (name, text, len) == 0)
      return strdup(name);
  }
  
  /* If no names matched, then return NULL. */
  return NULL;
}


/* Attempt to complete on the contents of TEXT.  START and END show the
   region of TEXT that contains the word to complete.  We can use the
   entire line in case we want to do some simple parsing.  Return the
   array of matches, or NULL if there aren't any. */
char **
cli_completion(const char *text, 
	       int start, 
	       int end) {
  char **matches;


  matches = (char **)NULL;

  /* If this word is at the start of the line, then it is a command
     to complete.  Otherwise it is the name of a file in the current
     directory. */
  if (start == 0)
    matches = rl_completion_matches(text, command_generator);

  return matches;
}



void
update_prompt(char *buf) {
  if (sel_room || sel_class || sel_comment || sel_type)
    strcpy(buf, "[");
  else
    buf[0] = '\0';
  
  if (order) {
    strcat(buf, " Order=");
    strcat(buf, order);
  }
	     
  if (sel_room) {
    strcat(buf, " Room=");
    strcat(buf, sel_room);
  }
	     
  if (sel_class) {
    strcat(buf, " Class=");
    strcat(buf, sel_class);
  }
	     
  if (sel_comment) {
    strcat(buf, " Comment=");
    strcat(buf, sel_comment);
  }
	     
  if (sel_type) {
    strcat(buf, " Type=");
    strcat(buf, endpoint_type2str(sel_type, 0));
  }

  if (sel_room || sel_class || sel_comment || sel_type)
    strcat(buf, " ] ");

  if (!write_f)
    strcat(buf, "PDB> ");
  else
    strcat(buf, "PDB# ");
}


void
header(void) {
  static int p_flag = 0;

  if (p_flag)
    return;

  printf("[PatchDB, version %s - Copyright (c) 2014 Peter Eriksson <peter@ifm.liu.se>]\n", version);
  p_flag = 1;
}


int
main(int argc,
     char *argv[]) {
  int i, j, rc, erc = 0;


  rc = 0;
  argv0 = argv[0];
  db_rw_user = getenv("USER");
  int n_patches = 0;
  int n_outlets = 0;
  int n_ports = 0;
  
  
  for (i = 1; i < argc && argv[i][0] == '-'; i++) {
    for (j = 1; argv[i][j]; j++)
      switch (argv[i][j]) {
      case 'P':
	if (argv[i][j+1]) {
	  db_rw_pass = strdup(argv[i]+j+1);
	  goto NextArg;
	} else if (i+1 < argc) {
	  db_rw_pass = strdup(argv[++i]);
	  goto NextArg;
	} else { 
	  fprintf(stderr, "%s: %s: Missing required argument\n", argv[0], argv[i]);
	  exit(1);
	}
	break;
	
      case 'U':
	if (argv[i][j+1]) {
	  db_rw_user = strdup(argv[i]+j+1);
	  goto NextArg;
	} else if (i+1 < argc) {
	  db_rw_user = strdup(argv[++i]);
	  goto NextArg;
	} else { 
	  fprintf(stderr, "%s: %s: Missing required argument\n", argv[0], argv[i]);
	  exit(1);
	}
	break;
	
      case 'C':
	if (argv[i][j+1]) {
	  sel_comment = strdup(argv[i]+j+1);
	  goto NextArg;
	} else if (i+1 < argc) {
	  sel_comment = strdup(argv[++i]);
	  goto NextArg;
	} else { 
	  fprintf(stderr, "%s: %s: Missing required argument\n", argv[0], argv[i]);
	  exit(1);
	}
	break;

      case 'c':
	if (argv[i][j+1]) {
	  sel_class = strdup(argv[i]+j+1);
	  goto NextArg;
	} else if (i+1 < argc) {
	  sel_class = strdup(argv[++i]);
	  goto NextArg;
	} else { 
	  fprintf(stderr, "%s: %s: Missing required argument\n", argv[0], argv[i]);
	  exit(1);
	}
	break;

      case 't':
	if (argv[i][j+1]) {
	  if ((sel_type = endpoint_str2type(argv[i]+j+1)) < 0) {
	    fprintf(stderr, "%s: %s: Invalid type\n", argv[0], argv[i]+j+1);
	    exit(1);
	  }
	  goto NextArg;
	} else if (i+1 < argc) {
	  if ((sel_type = endpoint_str2type(argv[++i])) < 0) {
	    fprintf(stderr, "%s: %s: Invalid type\n", argv[0], argv[i]);
	    exit(1);
	  }
	  goto NextArg;
	} else { 
	  fprintf(stderr, "%s: %s: Missing required argument\n", argv[0], argv[i]);
	  exit(1);
	}
	break;

      case 'R':
	if (argv[i][j+1]) {
	  sel_room = fix_room(argv[i]+j+1);
	  goto NextArg;
	} else if (i+1 < argc) {
	  sel_room = fix_room(argv[++i]);
	  goto NextArg;
	} else { 
	  fprintf(stderr, "%s: %s: Missing required argument\n", argv[0], argv[i]);
	  exit(1);
	}
	break;
	
      case 'O':
	if (argv[i][j+1]) {
	  order = strdup(argv[i]+j+1);
	  goto NextArg;
	} else if (i+1 < argc) {
	  order = strdup(argv[++i]);
	  goto NextArg;
	} else { 
	  fprintf(stderr, "%s: %s: Missing required argument\n", argv[0], argv[i]);
	  exit(1);
	}
	break;
	
      case 'd':
	++debug;
	break;
	
      case 'n':
	write_f = 0;
	break;
	
      case 'w':
	write_f = 1;
	break;
	
      case 'V':
	header();
	break;

      case 'v':
	++verbose;
	break;
	
      case 'f':
	++force;
	break;
	
      case 'h':
	usage();
	exit(0);
	
      case '-':
	++i;
	goto EndArg;

      default:
	fprintf(stderr, "%s: %s: Invalid option\n", argv[0], argv[i]);
	exit(1);
      }
  NextArg:;
  }

 EndArg:
  if (write_f) {
    if (!db_rw_pass)
      db_rw_pass = getpassphrase("Enter password: ");
    
    if (db_init(db_rw_user, db_rw_pass, db_name, db_host) < 0) {
      fprintf(stderr, "%s: db_init(): %s\n", argv[0], db_error());
      exit(1);
    }
  } else {
    if (db_init(db_ro_user, db_ro_pass, db_name, db_host) < 0) {
      fprintf(stderr, "%s: db_init(): %s\n", argv[0], db_error());

      if (!db_rw_pass)
	db_rw_pass = getpassphrase("Enter password: ");
      
      if (db_init(db_rw_user, db_rw_pass, db_name, db_host) < 0) {
	fprintf(stderr, "%s: db_init(): %s\n", argv[0], db_error());
	exit(1);
      }
      write_f = 1;
    }
  }

  if (i >= argc) {
    char *buf;
    char promptbuf[256];

    if (write_f == -1)
      write_f = 0;

    if (isatty(fileno(stdin))) {
      interactive = 1;
      header();

      n_patches = db_get_rows("patches", NULL);
      n_outlets = db_get_rows("endpoints", "type = 1");
      n_ports = db_get_rows("endpoints", "type = 3");

      printf("*** Interactive mode [%d patches, %d outlets, %d switch ports] ***\n",
	   n_patches, n_outlets, n_ports);
      
      rl_readline_name = "patchdb";
      rl_attempted_completion_function = cli_completion;

      update_prompt(promptbuf);
      
      while (erc >= 0 && (buf = readline(promptbuf))) {
	while (isspace(*buf))
	  ++buf;
	if (*buf) {
	  add_history(buf);
	  rc = cli_execute(buf);
	  if (rc != 0) {
	    erc = rc;
	  }
	  
	  update_prompt(promptbuf);
	}
      }
    } else {
      char cmdbuf[1024], *buf;

      while (erc >= 0 && fgets(cmdbuf, sizeof(cmdbuf), stdin)) {
	buf = cmdbuf;
	while (isspace(*buf))
	  ++buf;
	if (*buf) {
	  rc = cli_execute(buf);
	  if (rc != 0) {
	    erc = rc;
	  }
	}
      }
    }
  }
  else
    erc = cmd_execute(argc-i, argv+i);

  db_close();
  return erc > 0 ? 1 : 0;
}
