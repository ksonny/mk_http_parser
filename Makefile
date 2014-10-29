all:
	gcc -DHTTP_STANDALONE -g -Wall mk_http_parser.c test.c -o test

clean:
	rm -rf test *~ *.o
