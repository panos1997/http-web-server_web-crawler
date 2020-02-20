all : myhttpd

myhttpd : webserver.o
	gcc -o myhttpd webserver.o  -lpthread -g -lm

webserver.o : webserver.c
	gcc -c webserver.c -lpthread -g

clean :
	rm -rf *.o myhttpd
