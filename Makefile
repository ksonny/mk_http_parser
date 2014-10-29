all:
	gcc -DHTTP_STANDALONE -g -Wall mk_http.c test.c -o test

clean:
	rm -rf test *~ *.o
