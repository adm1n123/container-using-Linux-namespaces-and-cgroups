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
#define CGROUP_PIDS_DIR "/sys/fs/cgroup/pids/container/"
#define CGROUP_MEMORY_DIR "/sys/fs/cgroup/memory/container/"
#define CGROUP_CPU_DIR "/sys/fs/cgroup/cpu/container/"

const char *rootfs;
const char *hostname;
const int MAX_PID = 10;	// maximum number of processes in container.
const int MEM_LIMIT = 512 << 20; // 512MB

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

void set_hostname() {
	sethostname(hostname, strlen(hostname));
	return;
}

int write_on_file(char *path, char *data) {
	int fd = open(path, O_WRONLY|O_APPEND);
	if(fd < 0) {
		printf("Error opening file: %s\n", path);
		exit(1);
	}
	int flag = write(fd, data, strlen(data));
	close(fd);
	return flag;
}

void create_pids_cgroup() {
	mkdir(CGROUP_PIDS_DIR, S_IRWXU | S_IRWXO);
	char pid[50];
	sprintf(pid, "%d", getpid());

	char procs_path[100] = CGROUP_PIDS_DIR;
	strcat(procs_path, "cgroup.procs");
	write_on_file(procs_path, pid);	
	return;
}

void limit_pids(int max) {
	
	char max_pid[50];
	sprintf(max_pid, "%d", max);

	char pid_max_path[100] = CGROUP_PIDS_DIR;
	strcat(pid_max_path, "pids.max");
	write_on_file(pid_max_path, max_pid);
	return;
}

void notify_on_release(char *path) {	// after process ended kernel will clean up for space.
	char noti_path[100];
	sprintf(noti_path, "%s", path);
	strcat(noti_path, "notify_on_release");
	write_on_file(noti_path, "1");
	return;
}

void create_memory_cgroup() {
	mkdir(CGROUP_MEMORY_DIR, S_IRWXU | S_IRWXO);
	char pid[50];
	sprintf(pid, "%d", getpid());

	char tasks_path[100] = CGROUP_MEMORY_DIR;
	strcat(tasks_path, "tasks");
	int flag = write_on_file(tasks_path, pid);
	if(flag < strlen(pid)) {
		printf("create_memory_cgroup write failed flag: %d, and data length: %ld\n", flag, strlen(pid));
	}
	return;
}

void limit_memory(int bytes) {
	
	char mem_bytes[50];
	sprintf(mem_bytes, "%d", bytes);

	char memory_limit_path[100] = CGROUP_MEMORY_DIR;
	strcat(memory_limit_path, "memory.limit_in_bytes");
	int flag = write_on_file(memory_limit_path, mem_bytes);
	if(flag < strlen(mem_bytes)) {
		printf("limit_memory write failed flag: %d, and data length: %ld\n", flag, strlen(mem_bytes));
	}
	return;
}

char *stack_memory() {
	const int STACK_SIZE = 1<<20;	// 64KB
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

void limit_resources() {
	// limiting number of processes.
	create_pids_cgroup();
	limit_pids(MAX_PID);
	notify_on_release(CGROUP_PIDS_DIR);

	// limiting memory usages.
	create_memory_cgroup();
	limit_memory(MEM_LIMIT);
	notify_on_release(CGROUP_MEMORY_DIR);
	return;
}

int init(void *args) {
	printf("From init Child pid: %d\n", getpid());

	// create cgroup for this init process in host rootfs not container rootfs.

	limit_resources();


	init_env_variables();
	setup_rootfs();
	set_hostname();

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
