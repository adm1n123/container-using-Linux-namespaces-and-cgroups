
all:
	gcc -o container container.c

client:
	gcc -o client client.c

server:
	gcc -o server server.c -lpthread

