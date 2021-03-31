#define _GNU_SOURCE	// then only clone() is defined in sched.h
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sched.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <sys/wait.h>

// create difference cgroup for separate container when running two containers.
#define DIR_CGROUP_PIDS "/sys/fs/cgroup/pids/container/"
#define DIR_CGROUP_MEMORY "/sys/fs/cgroup/memory/container/"
#define DIR_CGROUP_CPU "/sys/fs/cgroup/cpu/container/"

#define PATH_SHELL "/bin/sh"
#define PATH_BASH "/bin/bash"

#define STR_CGROUP_PROCS "cgroup.procs"
#define STR_PIDS_MAX "pids.max"
#define STR_MEMORY_TASKS "tasks"
#define STR_MEMORY_LIMIT "memory.limit_in_bytes"
#define STR_SWAPPINESS "memory.swappiness"
#define STR_NOTI_ON_REL "notify_on_release"

const char *rootfs;
const char *hostname;
const int MAX_PID = 10;	// maximum number of processes in container.
const int PATH_LENGTH = 100;
const long long int MEM_LIMIT = (long long int)10 << 20; // 128MB
const long long int STACK_SIZE = (long long int)1 << 20; // 1MB

void initEnvVariables() {
  clearenv();	// clear environment variables for process it takes env from parent.
  setenv("TERM", "xterm-256color", 0);		// what type of screen we have
  setenv("PATH", "/bin/:/sbin/:usr/bin:/usr/sbin", 0);
  return;
}

char *concat(char *a, char *b) {
	char *p = malloc(PATH_LENGTH);
	strcpy(p, a);
	strcat(p, b);
	return p;
}

char *intToString(int n) {
	char *p = malloc(25);
	sprintf(p, "%d", n);
	return p;
}

char *longToString(long long int n) {
	char *p = malloc(25);
	sprintf(p, "%lld", n);
	return p;
}

void setupRootfs() {
	char *rootfs_path = concat("./", (char *)rootfs);
	chroot(rootfs_path);
	chdir("/");
	return;
}

void setHostname() {
	sethostname(hostname, strlen(hostname));
	return;
}

int writeOnFile(char *path, char *data) {
	int fd = open(path, O_WRONLY|O_APPEND);
	if(fd < 0) {
		printf("Error opening file: %s\n", path);
		exit(1);
	}
	int flag = write(fd, data, strlen(data));
	close(fd);
	return flag;
}

void createJoinPidsCgroup() {
	mkdir(DIR_CGROUP_PIDS, S_IRWXU | S_IRWXO);
	char *pid = intToString(getpid());

	char *path = concat(DIR_CGROUP_PIDS, STR_CGROUP_PROCS);
	writeOnFile(path, pid);
	return;
}

void limitPids(int max) {
	
	char *max_pid = intToString(max);
	char *path = concat(DIR_CGROUP_PIDS, STR_PIDS_MAX);
	writeOnFile(path, max_pid);
	return;
}

void notifyOnRelease(char *dir_cgroup) {	// after process ended kernel will clean up for space.
	char *path = concat(dir_cgroup, STR_NOTI_ON_REL);
	writeOnFile(path, "1");
	return;
}

void createJoinMemoryCgroup() {
	mkdir(DIR_CGROUP_MEMORY, S_IRWXU | S_IRWXO);
	char *pid = intToString(getpid());
	char *path = concat(DIR_CGROUP_MEMORY, STR_MEMORY_TASKS);

	int flag = writeOnFile(path, pid);
	if(flag < strlen(pid)) {
		printf("createMemoryCgroup write failed flag: %d, and data length: %ld\n", flag, strlen(pid));
	}
	return;
}

void limitMemory(long long int bytes) {
	
	char *mem_bytes = longToString(bytes);
	char *path = concat(DIR_CGROUP_MEMORY, STR_MEMORY_LIMIT);
	
	int flag = writeOnFile(path, mem_bytes);
	if(flag < strlen(mem_bytes)) {
		printf("limitMemory write failed flag: %d, and data length: %ld\n", flag, strlen(mem_bytes));
	}
	return;
}

void disableSwapping() {
	char *path = concat(DIR_CGROUP_MEMORY, STR_SWAPPINESS);
	int flag = writeOnFile(path, "0");	// default is 60 it is tendency of kernel to swap lower value decreases the tendency 0 means very low but kernel might swap.
	return;
}

char *stackMemory() {
	char *stack = malloc(STACK_SIZE);
	if(stack == NULL) {
		perror("stack memory allocation failed");
		exit(1);
	}
	return stack + STACK_SIZE;	// stack grow from high memory address to low hence give high address. clone loads the process on stack. eg. method calls.
	// how to control stackoverflow in this case?
}

int runShell() {
	char *args[] = {PATH_SHELL, NULL}; // {} is used to create array
	return execvp(PATH_SHELL, args);
}

int runBash() {
	char *args[] = {PATH_BASH, NULL}; // {} is used to create array
	return execvp(PATH_BASH, args);	// change to bash it will replace parent bash you need to do exit two times to exit from child bash and your initial bash.
}

int run(char *path) {
	char *args[] = {path, NULL}; // {} is used to create array
	return execvp(path, args);
}

void limitResources() {
	// limiting number of processes.

	createJoinPidsCgroup();
	limitPids(MAX_PID);
	notifyOnRelease(DIR_CGROUP_PIDS);
	printf("Process Limit: %d\n", MAX_PID);

	// limiting memory usages.
	createJoinMemoryCgroup();
	limitMemory(MEM_LIMIT);
	notifyOnRelease(DIR_CGROUP_MEMORY);
	disableSwapping();
	printf("Memory Limit: %lld MB\n", MEM_LIMIT/(1<<20));

	return;
}

int init(void *args) {
	printf("From init Child pid: %d\n", getpid());

	// create cgroup for this init process in host rootfs not container rootfs.

	limitResources();


	initEnvVariables();
	setupRootfs();
	setHostname();

	mount("proc", "/proc", "proc", 0, 0);	// mounting proc file system.

	clone(runShell, stackMemory(), SIGCHLD, NULL);
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
	
	clone(init, stackMemory(), CLONE_NEWPID|CLONE_NEWUTS|SIGCHLD, NULL); // SIGCHLD this flag tells the process to emit a signal when finished. NULL is args to init.
	
	int status;
	wait(&status);	// parent need to wait otherwise child process will be adopted.

	printf("Leaving main with init status: %d\n", status);
	return;
}
