.PHONY: clean valgrind

wordcount: distwc.o mapreduce.o threadpool.o
	gcc -Wall -Werror -std=c11 -pthread distwc.o mapreduce.o threadpool.o -o wordcount

valgrind: db_wordcount
	valgrind --tool=memcheck --leak-check=yes ./db_wordcount

db_wordcount: db_threadpool.o db_mapreduce.o db_distwc.o
	gcc -Wall -Werror -std=c11 -pthread -g -O0 db_threadpool.o db_mapreduce.o db_distwc.o -o db_wordcount

threadpool.o: threadpool.c
	gcc -Wall -Werror -std=c11 -pthread -c threadpool.c -o threadpool.o

mapreduce.o: mapreduce.c
	gcc -Wall -Werror -std=c11 -pthread -c mapreduce.c -o mapreduce.o

distwc.o: distwc.c
	gcc -Wall -Werror -std=c11 -pthread -c distwc.c -o distwc.o

db_threadpool.o: threadpool.c
	gcc -Wall -Werror -std=c11 -pthread -g -O0 -c threadpool.c -o db_threadpool.o

db_mapreduce.o: mapreduce.c
	gcc -Wall -Werror -std=c11 -pthread -g -O0 -c mapreduce.c -o db_mapreduce.o

db_distwc.o: distwc.c
	gcc -Wall -Werror -std=c11 -pthread -g -O0 -c distwc.c -o db_distwc.o

clean:
	rm -f wordcount *.o
