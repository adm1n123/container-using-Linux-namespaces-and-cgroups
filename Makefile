
container: container.c
	gcc -o container container.c

client: client.c
	gcc -o client client.c

server: server.c
	gcc -o server server.c -lpthread

