
INC=-I./include

all: proc1 proc2 contextmgr client

contextmgr: contextmgr.c ubinder.c logger.c
	gcc -static -lpthread -o contextmgr $(INC)  ./ubinder.c ./contextmgr.c ./logger.c

client: client.c ubinder.c logger.c
	gcc -static -lpthread -o client $(INC)  ./ubinder.c ./client.c ./logger.c

proc1: proc1.c ubinder.c logger.c
	gcc -static  -lpthread -o proc1 $(INC) ./proc1.c ./ubinder.c ./logger.c

proc2: proc2.c
	gcc -static -o proc2 $(INC) ./proc2.c

install:
	cp ./contextmgr ./client ./proc1 ./proc2 ../../home/

clean:
	rm test
