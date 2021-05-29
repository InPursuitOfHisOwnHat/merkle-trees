EXEC= mtree
LIBS= -lssl -lcrypto -lm
INCLUDES= -I ./cakelog/

LOGGER= ./cakelog/cakelog.o

all: ${LOGGER}
	gcc merkle_tree.c ${INCLUDES} ${LIBS} ${LOGGER} -o ${EXEC}

./cakelog.o:cakelog/cakelog.c
	gcc -c ./cakelog/cakelog.c -c -o ./cakelog.o

clean:
	rm -rf ./${EXEC} 
	rm -rf *.log
	rm ./cakelog/cakelog.o
