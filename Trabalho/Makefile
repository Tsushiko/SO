all: server client
server: execs/servidor

client: execs/cliente

execs/servidor: obj/servidor.o
	gcc -g obj/servidor.o -o execs/servidor

obj/servidor.o: src/servidor.c
	gcc -Wall -g -c src/servidor.c -o obj/servidor.o

execs/cliente: obj/cliente.o
	gcc -g obj/cliente.o -o execs/cliente

obj/cliente.o: src/cliente.c
	gcc -Wall -g -c src/cliente.c -o obj/cliente.o

clean:
	rm obj/* pipeserver/* pipeclient/* execs/{cliente,servidor}

test:
    ./execs/servidor ./max/transmax.conf ./trans
    ./execs/cliente proc-file ./input/teste.txt ./output/teste.txt bcompress
    ./execs/cliente proc-file ./output/teste.txt ./output/teste1.txt bdecompress
    ./execs/cliente status