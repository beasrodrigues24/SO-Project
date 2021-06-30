all: server client

server: bin/aurrasd

client: bin/aurras

bin/aurrasd: obj/aurrasd.o
	gcc -g obj/aurrasd.o -o bin/aurrasd

obj/aurrasd.o: src/aurrasd.c
	gcc -Wall -g -o obj/aurrasd.o -c src/aurrasd.c 

bin/aurras: obj/aurras.o
	gcc -g obj/aurras.o -o bin/aurras

obj/aurras.o: src/aurras.c
	gcc -Wall -g -o obj/aurras.o -c src/aurras.c 

clean:
	rm obj/*.o tmp/* bin/aurras bin/aurrasd

test:
	bin/aurras 
	bin/aurras status
	bin/aurras transform samples/sample-1-so.m4a output.m4a alto eco rapido

runServer: 
	bin/aurrasd etc/aurrasd.conf bin/aurrasd-filters

testOneFilter:
	bin/aurras transform samples/sample-1-so.m4a tmp/sample-1-so.mp3 eco

testSample1: 
	bin/aurras transform samples/sample-1-so.m4a tmp/sample-1-so.mp3 alto eco rapido

testeIevanPolka:	
	bin/aurras transform samples/Ievan-Polkka-Loituma.m4a tmp/Ievan.mp3 eco rapido

status: 
	bin/aurras status