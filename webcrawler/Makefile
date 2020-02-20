all : mycrawler

mycrawler : webcrawler.o
	gcc -o mycrawler webcrawler.o  -lpthread -g -lm

webcrawler.o : webcrawler.c
	gcc -c webcrawler.c -lpthread -g

clean :
	rm -rf *.o mycrawler
