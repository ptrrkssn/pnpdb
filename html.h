/*
** html.h
*/

#ifndef HTML_H
#define HTML_H 1

extern int
h_putc(int c);

extern int
h_puts(const char *str);

extern int
h_puts_br(const char *str);

extern void
h_header(const char *title,
	 ...);

extern void
h_footer(const char *footer,
	 ...);

extern void
h_a_start(const char *type,
	  const char *alt,
	  const char *ref,
	  ...);

extern void
h_a_end(void);

extern void
h_href(const char *label,
       const char *alt,
       const char *ref,
       ...);

extern int
h_email(const char *email);

extern int
h_putbody(const char *str);


extern void
h_button_submit(const char *name,
		const char *value,
		const char *title);

extern void
h_button_reset(const char *title);

extern void
h_td_start(const char *class,
	   int colspan);

extern void
h_td_end(void);

extern void
h_th_start(const char *class,
	   int colspan);

extern void
h_th_end(void);

extern void
h_td_int(int val,
	 const char *class,
	 int colspan);

extern void
h_td_str(const char *str,
	 const char *class,
	 int colspan);

extern void
h_td_int_field(const char *name,
	       int size,
	       int val,
	       const char *class,
	       int colspan);

extern void
h_td_empty_field(const char *name,
		 int size,
		 const char *class,
		 int colspan);


extern void
h_th_str(const char *str,
	 const char *class,
	 int colspan);

extern void
h_table_start(const char *class);

extern void
h_table_end(void);

extern void
h_tr_start(const char *class);

extern void
h_tr_end(void);

void
h_form_start(const char *method,
	     const char *action);

void
h_form_end();

extern void
h_form_int(const char *name,
	   const char *type,
	   int size,
	   int val);

extern void
h_form_str(const char *name,
	   const char *type,
	   int size,
	   char *val);

extern int
h_header_level(int lev);

extern void
h_form_set_row(unsigned int row);

extern void
h_form_set_row(unsigned int row);

#endif



