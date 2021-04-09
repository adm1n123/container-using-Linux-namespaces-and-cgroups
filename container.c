#define _GNU_SOURCE	// then only clone() is defined in sched.h
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sched.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <sys/wait.h>

// create difference cgroup for separate container when running two containers.
#define DIR_CGROUP_PIDS "/sys/fs/cgroup/pids/"
#define DIR_CGROUP_MEMORY "/sys/fs/cgroup/memory/"
#define DIR_CGROUP_CPU "/sys/fs/cgroup/cpu/"
#define DIR_NETNS "/var/run/netns/"

#define PATH_SHELL "/bin/sh"
#define PATH_BASH "/bin/bash"

#define STR_CGROUP_PROCS "cgroup.procs"
#define STR_PIDS_MAX "pids.max"
#define STR_MEMORY_TASKS "tasks"
#define STR_MEMORY_LIMIT "memory.limit_in_bytes"
#define STR_SWAPPINESS "memory.swappiness"
#define STR_NOTI_ON_REL "notify_on_release"

const char *CONTNR_ID;	// container id
const char *NETNS;		// network namespace
const char *ROOTFS;
const char *HOSTNAME;
const int MAX_PID = 10;	// maximum number of processes in container.
const int PATH_LENGTH = 100;
const long long int MEM_LIMIT = (long long int)10 << 20; // 10MB
const long long int STACK_SIZE = (long long int)1 << 20; // 1MB

char *concat(int argc, ...);
void networkConfig();

void initEnvVariables() {
  clearenv();	// clear environment variables for process it takes env from parent.
  setenv("TERM", "xterm-256color", 0);		// what type of screen we have
  setenv("PATH", "/bin/:/sbin/:usr/bin:/usr/sbin", 0);
  return;
}

char *concat(int argc, ...) {
	va_list args;
	va_start(args, argc);

	char *p = malloc(PATH_LENGTH);
	char *arg = va_arg(args, char *);
	strcpy(p, arg);

	for(int i = 2; i <= argc; i++) {
		arg = va_arg(args, char *);
		strcat(p, arg);
	}
	va_end(args);
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
	char *path = concat(2, "./", (char *)ROOTFS);
	chroot(path);
	chdir("/");
	return;
}

void setHostname() {
	sethostname(HOSTNAME, strlen(HOSTNAME));
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
	char *dir = concat(3, DIR_CGROUP_PIDS, CONTNR_ID, "/");
	mkdir(dir, S_IRWXU | S_IRWXO);
	char *pid = intToString(getpid());
	char *path = concat(4, DIR_CGROUP_PIDS, CONTNR_ID, "/", STR_CGROUP_PROCS);
	writeOnFile(path, pid);
	return;
}

void limitPids(int max) {
	char *max_pid = intToString(max);
	char *path = concat(4, DIR_CGROUP_PIDS, CONTNR_ID, "/", STR_PIDS_MAX);
	writeOnFile(path, max_pid);
	return;
}

void notifyOnRelease(char *dir_cgroup) {	// after process ended kernel will clean up for space.
	char *path = concat(4, dir_cgroup, CONTNR_ID, "/", STR_NOTI_ON_REL);
	writeOnFile(path, "1");
	return;
}

void createJoinMemoryCgroup() {
	char *dir = concat(3, DIR_CGROUP_MEMORY, CONTNR_ID, "/");
	mkdir(dir, S_IRWXU | S_IRWXO);
	char *pid = intToString(getpid());
	char *path = concat(4, DIR_CGROUP_MEMORY, CONTNR_ID, "/", STR_MEMORY_TASKS);

	int flag = writeOnFile(path, pid);
	if(flag < strlen(pid)) {
		printf("createMemoryCgroup write failed flag: %d, and data length: %ld\n", flag, strlen(pid));
	}
	return;
}

void limitMemory(long long int bytes) {
	
	char *mem_bytes = longToString(bytes);
	char *path = concat(4, DIR_CGROUP_MEMORY, CONTNR_ID, "/", STR_MEMORY_LIMIT);
	
	int flag = writeOnFile(path, mem_bytes);
	if(flag < strlen(mem_bytes)) {
		printf("limitMemory write failed flag: %d, and data length: %ld\n", flag, strlen(mem_bytes));
	}
	return;
}

void disableSwapping() {
	char *path = concat(4, DIR_CGROUP_MEMORY, CONTNR_ID, "/", STR_SWAPPINESS);
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

int run() {
	char *path = "/usr/bin/bash";
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

void joinNetworkNamespace() {
	char *path = concat(2, DIR_NETNS, NETNS);
	int nsfd = open(path, O_RDONLY | O_CLOEXEC);	// use O_CLOEXEC flag to prevent any child/exec processes from inheriting this fd. since namespace is always inherited.
	if(nsfd < 0) {
		printf("Error opening netns: %s\n", NETNS);
		exit(1);
	} 
	int flag = setns(nsfd, CLONE_NEWNET);	// join network namespace referred by nsfd.
	if(flag < 0) {
		printf("Error joining netns: %s\n", NETNS);
		exit(1);
	}
	printf("netns: %s joined by init process with PID: %d\n", NETNS, getpid());
	return;
}

void mountFS() {
	// if(mount("/", "/", NULL, MS_PRIVATE, NULL) < 0) {
	// 	printf("Error making root private mount\n");
	// 	exit(1);
	// }
	mkdir("/mnt/mounted_bin", S_IRWXU | S_IRWXO);
	if(mount("/bin", "/mnt/mounted_bin", NULL, MS_BIND, NULL) < 0) {
		printf("Mounting bin in home failed\n");
	}

	if(mount("proc", "/proc", "proc", 0, NULL) < 0) {	// mounting proc file system.
		printf("Mounting proc failed\n");
		exit(1);
	}

	return;
}
void umountFS() {
	umount("/proc");
	umount("/mnt/mounted_bin");
	return;
}

int init(void *args) {
	printf("Container init pid: %d\n", getpid());

	// create cgroup for this init process in host rootfs not container rootfs.
	limitResources();
	joinNetworkNamespace();

	initEnvVariables();
	setupRootfs();
	setHostname();

	mountFS();
	
	clone(runBash, stackMemory(), SIGCHLD, NULL);
	int status;
	wait(&status);
	
	umountFS();
	printf("Leaving init with bash status: %d\n", status);
	return 0;
}

void main(int argc, char const *argv[]) {	// since clone is done then name of this process and name of cloned init() process both are ./container.
	printf("Hello Container parent pid: %d\n", getpid());
	if(argc < 2) {
		printf("Invalid args, rootfs and HOSTNAMEs required\n");
		exit(1);
	}
	networkConfig();

	ROOTFS = argv[1];
	HOSTNAME = argv[2];
	CONTNR_ID = argv[3];
	NETNS = concat(2, "netns-", CONTNR_ID);

	printf("ROOTFS is: %s, HOSTNAME is: %s, NETNS is: %s\n", ROOTFS, HOSTNAME, NETNS);

	clone(init, stackMemory(), CLONE_NEWNS|CLONE_NEWPID|CLONE_NEWUTS|SIGCHLD, NULL); // SIGCHLD this flag tells the process to emit a signal when finished. NULL is args to init.
	
	int status;
	wait(&status);	// parent need to wait otherwise child process will be adopted.

	printf("Leaving main with container init status: %d\n", status);
	return;
}


void networkConfig() {
	// creating network namespaces
	system("sudo ip netns add netns-contnr1");
	system("sudo ip netns add netns-contnr2");

	// creating bridge in default network NS
	system("sudo ip link add v-net-contnr type bridge");

	// creating two veth cable to connect both network NS to bridge.
	system("sudo ip link add veth-contnr1 type veth peer name veth-contnr1-br");
	system("sudo ip link add veth-contnr2 type veth peer name veth-contnr2-br");
	
	// connecting veth between first network NS and bridge
	system("sudo ip link set veth-contnr1 netns netns-contnr1");
	system("sudo ip link set veth-contnr1-br master v-net-contnr");
	// connecting veth between second network NS and bridge
	system("sudo ip link set veth-contnr2 netns netns-contnr2");
	system("sudo ip link set veth-contnr2-br master v-net-contnr");

	// enabling the veth interface on both network NS
	system("sudo ip netns exec netns-contnr1 ip link set veth-contnr1 up");
	system("sudo ip netns exec netns-contnr2 ip link set veth-contnr2 up");

	// starting the router.
	system("sudo ip link set dev v-net-contnr up");

	// enabling the veth interface on bridge
	system("sudo ip link set veth-contnr2-br up");
	system("sudo ip link set veth-contnr1-br up");

	// assigning IP address to interface of both network NS
	system("sudo ip netns exec netns-contnr1 ip addr add 192.168.55.1/24 dev veth-contnr1");
	system("sudo ip netns exec netns-contnr2 ip addr add 192.168.55.2/24 dev veth-contnr2");
	// assigning IP address to router interface in default NS.
	system("sudo ip addr add 192.168.55.3/24 brd + dev v-net-contnr");

	return;
}