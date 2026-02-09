guestbook.cgi: guestbook.c
	gcc guestbook.c -o guestbook.cgi -I cgic -L cgic -I sqlite3 -L sqlite3 -lcgic -lsqlite3 -pthread

clean:
	rm guestbook.cgi

.phony: clean

