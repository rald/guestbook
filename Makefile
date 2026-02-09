all: cgic/libcgic.a sqlite3/libsqlite3.a guestbook.cgi

cgic/libcgic.a: cgic/cgic.c cgic/cgic.h
	gcc -c cgic/cgic.c -o cgic/cgic.o
	ar rcs cgic/libcgic.a cgic/cgic.o

sqlite3/libsqlite3.a: sqlite3/sqlite3.c sqlite3/sqlite3.h
	gcc -c sqlite3/sqlite3.c -o sqlite3/sqlite3.o
	ar rcs sqlite3/libsqlite3.a sqlite3/sqlite3.o

guestbook.cgi: guestbook.c
	gcc guestbook.c -o guestbook.cgi -I cgic -L cgic -I sqlite3 -L sqlite3 -lcgic -lsqlite3 -pthread

clean:
	rm guestbook.cgi

clean-all:
	rm guestbook.cgi sqlite3/libsqlite3.a cgic/libcgic.a
	rm sqlite3/sqlite3.o
	rm cgic/cgic.o

.phony: clean clean-all
