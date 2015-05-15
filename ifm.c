/* ifm.c - Contains IFM-specific code */
   
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "ifm.h"


int
valid_room(const char *name)
{
    int n;
    char *cp, c;
    

    if (!name)
	return 0;
    
    if (strlen(name) < 6)
	return 0;

    if (name[1] != ' ')
	return 0;

    switch (name[0])
    {
      case 'M':
	name += 2;
	if (name[0] < '0' || name[0] > '9')
	    return 0;
	if (name[1] != ':')
	    return 0;
	
	if (!isdigit(name[2]) ||
	    !isdigit(name[3]) ||
	    !isdigit(name[4]))
	    return 0;
	if (name[5])
	    return 0;
	return 1;

      case 'F':
	name += 2;
	if (name[0] < 'A' || name[0] > 'Z')
	    return 0;
	if (!isdigit(name[1]) ||
	    !isdigit(name[2]) ||
	    !isdigit(name[3]))
	    return 0;
	if (name[4])
	    return 0;
	return 1;

    case 'B': /* B 221:193 / B 2C:643 / B 223C */
	name += 2;
	if (name[0] < '1' || name[0] > '3')
	    return 0;

	cp = strchr(name, ':');
	if (cp) {
	  switch (cp-name) {
	  case 2:
	    if (name[1] < 'A' || name[1] > 'E')
	      return 0;
	    break;
	    
	  case 3:
	    if (sscanf(name+1, "%u", &n) != 1)
	      return 0;
	    
	    if (n < 21 || n > 25)
	      return 0;
	    break;
	    
	  default:
	    return 0;
	  }
	  
	  ++cp;
	  if (sscanf(cp, "%u", &n) != 1)
	    return 0;
	  
	  if (n < 100 || n > 999)
	    return 0;
	} else {
	  if (sscanf(name, "%u%c", &n, &c) != 2)
	    return 0;

	  if (n < 100 || n > 999)
	    return 0;
	  
	  if (c < 'A' || c > 'E')
	    return 0;
	}
	
	return 1;
    }
    
    return 0;
}

int
valid_port(const char *port)
{
    unsigned int h, r, p, o;
    char c, t;
    

    if (!port)
	return 0;

    c = 0;
    switch (strlen(port))
    {
      case 8:
	if (sscanf(port, "%2u%2u%2u%2u%c", &h, &r, &p, &o, &c) != 4)
	    return 0;

	if (h != 2) /* House B */
	    return 0;
	if (!(r == 21 || r == 27 || r == 33 || r == 39))
	    return 0;
	if (p < 1 || p > 9)
	    return 0;
	if (o < 1 || p > 32)
	    return 0;
	return 1;

      case 9:
	if ((port[0] == 'A' && port[1] == 'B') ||
	    (port[0] == 'B' && (port[1] >= 'A' && port[1] <= 'Z')))
	{
	    if (sscanf(port+2, "%2u%2u%c%2u%c", &r, &p, &t, &o, &c) != 4)
		return 0;
	    if (r < 1 || r > 3)
		return 0;
	    if (p < 1 || p > 20)
		return 0;
	    if (o < 1 || o > 32)
		return 0;

	    return 1;
	}
	else if (port[0] == '0' && port[1] == '2')
	{
	    if (sscanf(port+2, "%2u%2u%c%2u%c", &r, &p, &t, &o, &c) != 4)
		return 0;
	    if (r < 1 || r > 50)
		return 0;
	    if (p < 1 || p > 20)
		return 0;
	    if (o < 1 || o > 32)
		return 0;

	    return 1;
	}
	else
	    return 0;
    }

    return 0;
}

char *
fix_room(const char *name)
{
    char *buf, *cp;
    int len;
    
    if (!name)
	return NULL;

    len = strlen(name);
    if (isdigit(name[0]))
    {
	if (name[1] == ':' || name[1] == '.')
	{
	    /* Building M (1:113 or 1.113) */
	    buf = malloc(len+3);
	    strcpy(buf, "M ");
	    strcat(buf, name);
	    buf[3] = ':';
	}
	else
	{
	    /* Building B (221:233, 3C579 etc)*/
	    buf = malloc(len+3);
	    strcpy(buf, "B ");
	    strcat(buf, name);
	}
    }
    else if (isalpha(name[0]) && isspace(name[1]))
    {
	buf = strdup(name);
	cp = buf;
	
	if (buf[0] == 'M' && isdigit(buf[2]) && buf[3] == '.')
	    buf[3] = ':';
    }
    else
    {
	/* Building F (A119, J202 etc) */
	buf = malloc(len+3);
	strcpy(buf, "F ");
	strcat(buf, name);
    }
    
    for (cp = buf; *cp; cp++)
	if (islower(*cp))
	    *cp = toupper(*cp);

    /* Only support B or F for now... */
    if (!((buf[0] == 'B' || buf[0] == 'F' || buf[0] == 'M') && buf[1] == ' '))
	return NULL;

    /* Remove extra whitespace */
    for (cp = buf+2; *cp && isspace(*cp); ++cp)
	;
    if (cp > buf+2)
	strcpy(buf+2, cp);
    buf[1] = ' ';
    
    if (strncmp(buf, "B B", 3) == 0)
    {
	strcpy(buf+2, buf+3);
    }

    return buf;
}

char *
fix_port(const char *name)
{
    char *buf;
    char *to;
    const char *from;


    if (!name)
	return NULL;
    
    buf = malloc(strlen(name)+1);
    to = buf;
    from = name;
    
    while (*from)
    {
	while (*from == ' ' || *from == '-')
	    ++from;

	*to++ = *from++;
    }

    *to = '\0';
    
    if (strlen(buf) == 9 && buf[6] == 'A' &&
	(strncmp(buf, "0221", 4) == 0 ||
	 strncmp(buf, "0227", 4) == 0 ||	
	 strncmp(buf, "0233", 4) == 0 ||
	 strncmp(buf, "0239", 4) == 0))
    {

	buf[6] = buf[7];
	buf[7] = buf[8];
	buf[8] = '\0';
    }

    if (strncmp(buf, "18", 2) == 0)
	strcpy(buf, buf+2);
    
    return buf;
}



