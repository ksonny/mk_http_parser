CFLAGS := -DHTTP_STANDALONE -g -Wall -Wextra

all: test1 test2

test2: CFLAGS += -DTEST2
test2: mk_http_parser2.o test.c
	$(CC) $(CFLAGS) $^ -o $@

test1: CFLAGS += -DTEST1
test1: mk_http_parser.o test.c
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -rf test1 test2 *~ *.o
