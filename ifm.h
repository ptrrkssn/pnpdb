/* ifm.h */

#ifndef IFM_H
#define IFM_H 1

extern int
valid_room(const char *name);

extern int
valid_port(const char *port);

extern char *
fix_room(const char *name);

extern char *
fix_port(const char *name);

#endif
