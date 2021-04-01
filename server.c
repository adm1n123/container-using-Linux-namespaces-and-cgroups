/*
This server program echoes the client query. Multiple clients are supported.
Number of worder threads can be specified for handling client query. Main process is
handling listening socket in main() mehtod.

Each worker thread is handling one epoll instance in this design(single worker thread can handle many epoll instances), one epoll instance is handling many socket fds for events.
new client's connection request is allocated to worker threads in round robbin fashion.

*/

#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <sys/socket.h> 
#include <sys/types.h> 
#include <netdb.h> 
#include <netinet/in.h> 
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <pthread.h>


// function prototypes
void *echo(void *thread_no); // server method to echo the client query. we can prepare server response for query.
void init_epolls_threads(); // creating threads and creating epoll instance for each thread.
void make_non_block_socket(int fd); // make the socket fd non blocking so that read/write on fd can be performed without blocking.
int create_lstn_sock_fd(); // create listening socket.


int n_threads = 2; // number of threads handling clients requests.

struct my_epoll_context { // this is custom structure used for data storation.
	int epoll_fd; // this is file descriptor of epoll instance. we will add remove socket fds using this epoll_fd.
	struct epoll_event *response_events; // when we wait on epoll then list of event will be returned(of type 'struct epoll_event') and we will store those in this memory (NOTE: we have already created memory for this pointer).
	// you can store response events anywhere but it is good to store the data related to same epoll in same structure.
};

struct my_epoll_context *epolls; // each thread has one epoll instance and all the socket fds allocated to thread will be in thread's epoll instance. epoll instance can have many socket/file fds to detect events.


void *echo(void *thread_no) {
	int thread_idx = *(int *)thread_no;

	char buff[50];
	int nfds, len;
	while(1) {
		nfds = epoll_wait(epolls[thread_idx].epoll_fd, epolls[thread_idx].response_events, 10, -1);// 10 is the maxevents to be returned by call (we have allocated space for 10 events during epoll instance creation you can increase) -1 timeout means it will never timeout means call returns in case of events/interrupts.
		for(int i = 0; i < nfds; i++) {
			int sock_fd = epolls[thread_idx].response_events[i].data.fd;

			bzero(buff, sizeof(buff)); // initialize buffer with ascii zero('\0')
			len = read(sock_fd, buff, sizeof(buff));
			if(len == 0) { // event occured but client didn't query means client disconnected.
				close(sock_fd);
				printf("socket fd: %d closed of thread no: %d\n", sock_fd, thread_idx);
				continue;
			}
			printf("Client fd %d: %s",sock_fd, buff);
			write(sock_fd, buff, sizeof(buff));
		}
	}
}

void init_epolls_threads() {
	pthread_t workers[n_threads]; // worker threads.
	epolls = (struct my_epoll_context *) malloc(n_threads * sizeof(struct my_epoll_context)); // array of epoll_context and each context is handled by one thread.
	for(int i = 0; i < n_threads; i++) {
		epolls[i].epoll_fd = epoll_create1(0); // creating the epoll instance for this thread it returns the epoll instance fd.
		epolls[i].response_events = calloc(10, sizeof(struct epoll_event)); // this memory location will be passed to epoll_wait to write the response events.

		int *thread_no = (int *) malloc(sizeof(int));
		*thread_no = i;
		pthread_create(&workers[i], NULL, &echo, (void *)thread_no); // creating the thread
	}
}

void make_non_block_socket(int fd) {
	int flags = fcntl(fd, F_GETFL, 0); // getting current flags of socket. F_GETFL is get flag command.
	flags |= O_NONBLOCK; // adding one more flag to socket. F_SETFL is set flag command.
	flags = fcntl(fd, F_SETFL, flags); // setting the new flag.
	if(flags == -1) {
		printf("non block failed for fd: %d", fd);
		exit(0);
	}
}

int create_lstn_sock_fd() {
	int flag;
	
	struct sockaddr_in lstn_socket, client_addr;
	
	int lstn_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(lstn_sock_fd == -1) {
		printf("listening socket creation failed\n");
		exit(0);
	} else printf("listening socket created\n");

	lstn_socket.sin_family = AF_INET;
	lstn_socket.sin_addr.s_addr = htonl(INADDR_ANY);
	lstn_socket.sin_port = htons(8080);

	flag = bind(lstn_sock_fd, (struct sockaddr *)&lstn_socket, sizeof(lstn_socket));
	if(flag == -1) {
		printf("Bind failed\n");
		exit(0);
	} else printf("Bind successful\n");

	flag = listen(lstn_sock_fd, 5);
	if(flag == -1) {
		printf("Error listening on socket\n");
		exit(0);
	} else printf("listening...\n");
	return lstn_sock_fd;
}

int main() {
	int clnt_sock_fd, len, flag, turn = 0;
	int lstn_sock_fd; // listening socket fd

	lstn_sock_fd = create_lstn_sock_fd();
	init_epolls_threads();

	while(1) {
		struct sockaddr_in client_addr;
		len = sizeof(client_addr);
		// accept(listen_fd, NULL, NULL); we can use this also because we are not using client IP,port etc. hence no point in passing second argument. second argument is reference to structure in which connected clients IP, port is stored.
		clnt_sock_fd = accept(lstn_sock_fd, (struct sockaddr *)&client_addr, &len);
		if(clnt_sock_fd == -1) {
			printf("Error accepting: error:%d\n", clnt_sock_fd);
			//exit(0);
			continue;
		} else printf("connection accepted\n");

		// for EPOLLET events it is advisable to use non-blocking operations on fd eg. read/write on socket.
		make_non_block_socket(clnt_sock_fd);
		
		struct epoll_event interested_event; // struct epoll_event is inbuilt structure we just created variable of this struct type to store interested event data for this epoll instance.
		interested_event.data.fd = clnt_sock_fd; // adding the socket fd
		interested_event.events = EPOLLIN | EPOLLET; // adding the event type for this socket fd.
		epoll_ctl(epolls[turn].epoll_fd, EPOLL_CTL_ADD, clnt_sock_fd, &interested_event); // adding the socket to epoll instance.
		printf("socket fd:%d added to thread no: %d\n", clnt_sock_fd, turn);
		turn = (turn + 1) % n_threads;
	}
	close(lstn_sock_fd);
}