# Makefile for PatchDB

CC=gcc -g -m32 -O -Wall
CFLAGS=-I/ifm/include -I/ifm/include/mysql
LDFLAGS=-R/ifm/lib -L/ifm/lib -lmysqlclient -lreadline -lcurses


CLIDEST=/ifm/bin
CGIDEST=/ifm/var/www/admin/htdocs/patches


PGMS=pnpdb.cgi pnpdb

COMOBJS=dbmisc.o plist.o version.o ifm.o
CGIOBJS=pnpdb-cgi.o $(COMOBJS) form.o html.o
CLIOBJS=pnpdb-cli.o $(COMOBJS) html.o


all:		$(PGMS)

pnpdb.cgi:	$(CGIOBJS)
	$(CC) -o pnpdb.cgi $(CGIOBJS) $(LDFLAGS)

pnpdb: 	$(CLIOBJS)
	$(CC) -o pnpdb $(CLIOBJS) $(LDFLAGS)
	
version:
	(PACKNAME=`basename \`pwd\`` ; echo 'char version[] = "'`echo $$PACKNAME | cut -d- -f2`'";' >version.c)

clean distclean:	version
	-rm -f $(PGMS) core *.o *~ \#*

install:	all
	cp pnpdb $(CLIDEST)
	cp pnpdb.cgi $(CGIDEST)

	
plist.o: 	plist.c plist.h
form.o: 	form.c form.h
html.o: 	html.c html.h
dbmisc.o: 	dbmisc.c dbmisc.h
pnpdb-cgi.o: 	pnpdb-cgi.c dbmisc.h plist.h form.h html.h
pnpdb-cli.o: 	pnpdb-cli.c dbmisc.h plist.h


