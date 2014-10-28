all:
	gcc -DHTTP_STANDALONE -g -Wall mk_http.c -o test
