/*
** html.c
*/

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include "html.h"

static unsigned int t_row = 0;


static int header_printed = 0;

static int header_level = 2;
static char *header_title = "???";


int
h_putc(int c)
{
    int rc;

  
    switch (c)
    {
      case '<':
	rc = fputs("&lt;", stdout);
	break;
      
      case '>':
	rc = fputs("&gt;", stdout);
	break;
      
      case '&':
	rc = fputs("&amp;", stdout);
	break;
      
      case '"':
	rc = fputs("&quot;", stdout);
	break;

      default:
	if (!iscntrl(c) || c == '\t' || c == '\r' || c == '\n')
	    rc = putchar(c);
	else
	    rc = putchar('?');
    }

    return rc;
}


int
h_puts(const char *str)
{
    int rc = 0;
  
    if (!str)
	return 0;
  
    while (*str && rc != EOF)
	rc = h_putc(*str++);

    return rc;
}

int
h_puts_br(const char *str)
{
    int rc = 0;
  
    if (!str)
	return 0;
  
    while (*str && rc != EOF)
    {
	if (*str == '\n')
	    fputs("<br>", stdout);
	else
	    rc = h_putc(*str);
	++str;
    }

    return rc;
}

int
h_putbody(const char *str)
{
    int rc = 0;
  
    if (!str)
	return 0;
  
    while (*str && rc != EOF)
    {
	switch (*str)
	{
	  case '\n':
	    rc = fputs("<br>", stdout);
	    break;

	  default:
	    rc = h_putc(*str);
	}
	++str;
    }

    return rc;
}

static int
filesend(const char *path,
	 FILE *out)
{
    FILE *in;
    int c;
    
    
    in = fopen(path, "r");
    if (!in)
	return -1;
    
    while ((c = getc(in)) != EOF)
	putc(c, out);
    
    fclose(in);
    return 0;
}


int
h_header_level(int lev)
{
    int old = header_level;
    
    header_level = lev;
    return old;
}

char *
h_header_title(const char *txt)
{
    char *old = header_title;
    
    header_title = strdup(txt);
    return old;
}

void
h_header(const char *subject,
	 ...)
{
    char *path = getenv("HEADER");
    va_list ap;
    char buf[1024];
    

    if (header_printed)
	return;
    
    va_start(ap, subject);
    buf[0] = 0;
    if (subject)
	vsnprintf(buf, sizeof(buf)-1, subject, ap);
    
    if (!path || filesend(path, stdout) < 0)
    {
	puts("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\" \"http://www.w3.org/TR/html4/loose.dtd\">");
	puts("<html>");
	puts("<head>");
	if (header_title)
	{
	    printf("<title>");
	    h_puts(header_title);
	    puts("</title>");
	}
	puts("</head>");
	puts("<body>");
    }
    
    if (subject && header_level > 0)
    {
	printf("<h%d>", header_level);
	h_puts(buf);
	printf("</h%d>\n", header_level);
    }
    
    va_end(ap);

    header_printed = 1;
}

void
h_footer(const char *footer,
	 ...)
{
    char *path = getenv("FOOTER");
    va_list ap;
    char buf[1024];


    if (!header_printed)
	return;
    
    va_start(ap, footer);
    buf[0] = 0;
    if (footer)
    {
	vsnprintf(buf, sizeof(buf)-1, footer, ap);
	puts("<div id=\"footer\">");
	h_puts(buf);
	puts("</div>");

    }
    
    if (path)
	filesend(path, stdout);
    else
    {
	puts("</body>");
	puts("</html>");
    }
}


void
h_a_start(const char *type,
	  const char *alt,
	  const char *ref,
	 ...)
{
    int rc;
    char buf[10240];
    va_list ap;
  

    va_start(ap, ref);
    vsnprintf(buf, sizeof(buf), ref, ap);
    va_end(ap);

    rc = fputs("<a", stdout);
    if (rc < 0)
	return;

    if (alt)
    {
	rc = fputs(" title=\"", stdout);
	if (rc < 0)
	    return;
      
	h_puts(alt);
      
	rc = fputs("\"", stdout);
	if (rc < 0)
	    return;
    }
  
    rc = printf(" %s=\"", type);
    if (rc < 0)
	return;
  
    h_puts(buf);
  
    rc = fputs("\">", stdout);
}

void
h_a_end(void)
{
    fputs("</a>", stdout);
}


void
h_href(const char *label,
       const char *alt,
       const char *ref,
       ...)
{
    h_a_start("href", alt, ref);
    if (label)
    {
	h_puts(label);
	h_a_end();
    }
}


int
h_email(const char *email)
{
    const char *cp;
    int rc = 0;
    

    for (cp = email; rc >= 0 && *cp; ++cp)
	switch (*cp)
	{
	  case '.':
	    rc = fputs(" . ", stdout);
	    break;
	    
	  case '@':
	    rc = fputs(" (vid) ", stdout);
	    break;
	    
	  default:
	    rc = h_putc(*cp);
	}
    
    return rc;
}


void
h_button_submit(const char *name,
		const char *value,
		const char *title)
{
    fputs("<input type=\"submit\" name=\"", stdout);
    h_puts(name);
    fputs("\" value=\"", stdout);
    h_puts(title);
    fputs("\">\n", stdout);
}


void
h_button_reset(const char *title)
{
    fputs("<input type=\"reset\" value=\"", stdout);
    h_puts(title);
    fputs("\">\n", stdout);
}

static char form_row[64] = "";

void
h_form_set_row(unsigned int row)
{
    if (row > 0)
	sprintf(form_row, "%u", row);
    else
	strcpy(form_row, "");
}

void
h_form_start(const char *method,
	     const char *action)
{
    fputs("<form method=\"", stdout);
    h_puts(method);
    fputs("\" action=\"", stdout);
    h_puts(action);
    puts("\">");
}

void
h_form_end(void)
{
    puts("</form>");
}

void
h_form_int(const char *name,
	   const char *type,
	   int size,
	   int val)
{
    printf("<input type=\"%s\" name=\"%s%s\"", type, name, form_row);
    
    if (size > 0)
	printf(" size=\"%d\"", size);
    
    printf(" value=\"%d\">", val);
}


void
h_form_str(const char *name,
	   const char *type,
	   int size,
	   char *val)
{
    printf("<input type=\"%s\" name=\"%s%s\"", type, name, form_row);
    
    if (size > 0)
	printf(" size=\"%d\"", size);

    if (val)
    {
	fputs(" value=\"", stdout);
	h_puts(val);
	fputs("\"", stdout);
    }

    putchar('>');
}


void
h_td_start(const char *class,
	   int colspan)
{
    fputs("<td", stdout);
    if (class)
	printf(" class=\"%s\"", class);
    if (colspan > 1)
	printf(" colspan=\"%d\"", colspan);
    fputs(">", stdout);
}

void
h_td_end(void)
{
    fputs("</td>\n", stdout);
}

void
h_th_start(const char *class,
	   int colspan)
{
    fputs("<th", stdout);
    if (class)
	printf(" class=\"%s\"", class);
    if (colspan > 1)
	printf(" colspan=\"%d\"", colspan);
    fputs(">", stdout);
}


void
h_th_end(void)
{
    fputs("</th>\n", stdout);
}

void
h_td_int(int val,
	 const char *class,
	 int colspan)
{
    h_td_start(class, colspan);
    printf("%d", val);
    h_td_end();
}

void
h_td_str(const char *str,
	 const char *class,
	 int colspan)
{
    h_td_start(class, colspan);
    if (str)
	h_puts(str);
    else
	fputs("&nbsp;", stdout);
    h_td_end();
}


void
h_td_int_field(const char *name,
	       int size,
	       int val,
	       const char *class,
	       int colspan)
{
    h_td_start(class, colspan);
    h_form_int(name, "text", size, val);
    h_td_end();
}

void
h_td_empty_field(const char *name,
		 int size,
		 const char *class,
		 int colspan)
{
    h_td_start(class, colspan);
    h_form_str(name, "text", size, NULL);
    h_td_end();
}

void
h_th_str(const char *str,
	 const char *class,
	 int colspan)
{
    h_th_start(class, colspan);
    h_puts(str);
    h_th_end();
}


void
h_tr_start(const char *class)
{
    if (!class)
	class = (t_row & 1) ? "odd" : "even";
    
    ++t_row;
    
    fputs("<tr", stdout);
    fprintf(stdout, " class=\"%s\"", class);
    fputs(">\n", stdout);
}

void
h_tr_end(void)
{
    fputs("</tr>\n", stdout);
}

void
h_table_start(const char *class)
{
    t_row = 0;
    
    fputs("<table", stdout);
    if (class)
	printf(" class=\"%s\"", class);
    fputs(">\n", stdout);
}

void
h_table_end(void)
{
    fputs("</table>\n", stdout);
}
