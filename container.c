#define _GNU_SOURCE	// then only clone() is defined in sched.h
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <sys/wait.h>

const char *rootfs;
const char *hostname;

void init_env_variables() {
  clearenv();	// clear environment variables for process it takes env from parent.
  setenv("TERM", "xterm-256color", 0);		// what type of screen we have
  setenv("PATH", "/bin/:/sbin/:usr/bin:/usr/sbin", 0);
  return;
}

void setup_rootfs() {
	char rootfs_path[50] = "./";
	strcat(rootfs_path, rootfs);

	chroot(rootfs_path);
	chdir("/");
	return;
}

char *stack_memory() {
	const int STACK_SIZE = 65536;	// 64KB
	char *stack = malloc(STACK_SIZE);
	if(stack == NULL) {
		perror("stack memory allocation failed");
		exit(1);
	}
	return stack + STACK_SIZE;	// stack grow from high memory address to low hence give high address. clone loads the process on stack. eg. method calls.
	// how to control stackoverflow in this case?
}

int run_shell() {
	char *path = "/bin/sh";
	char *args[] = {path, NULL}; // {} is used to create array
	return execvp(path, args);
}

int run_bash() {
	char *path = "/bin/bash";
	char *args[] = {path, NULL}; // {} is used to create array
	return execvp(path, args);	// change to bash it will replace parent bash you need to do exit two times to exit from child bash and your initial bash.
}

int run(char *path) {
	char *args[] = {path, NULL}; // {} is used to create array
	return execvp(path, args);
}

int init(void *args) {
	printf("From init\n");
	printf("Child pid: %d\n", getpid());

	init_env_variables();
	setup_rootfs();

	mount("proc", "/proc", "proc", 0, 0);	// mounting proc file system.

	clone(run_shell, stack_memory(), SIGCHLD, NULL);
	int status;
	wait(&status);
	
	umount("/proc");
	printf("Leaving init child status: %d\n", status);
	return 0;
}

void main(int argc, char const *argv[]) {	// since clone is done then name of this process and name of cloned init() process both are ./container.
	if(argc < 2) {
		printf("Invalid args, rootfs and hostnames required\n");
		exit(1);
	}
	rootfs = argv[1];
	hostname = argv[2];
	printf("rootfs is: %s, hostname is: %s\n", rootfs, hostname);

	printf("Hello Container parent pid: %d\n", getpid());
	
	clone(init, stack_memory(), CLONE_NEWPID|CLONE_NEWUTS|SIGCHLD, NULL); // SIGCHLD this flag tells the process to emit a signal when finished. NULL is args to init.
	
	int status;
	wait(&status);	// parent need to wait otherwise child process will be adopted.

	printf("Leaving main with init status: %d\n", status);
	return;
}
