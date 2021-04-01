#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

const int NUM_EAT = 1e5;
const long long int EAT_SIZE = (long long int)1 << 20; // 1MB

void hungry() {
	long long int total = 0;
	char **arr = (char **)malloc(NUM_EAT);
	// total += sizeof(arr[0]) * NUM_EAT;

	for(int i = 0; i < NUM_EAT; i++) {
		arr[i] = malloc(EAT_SIZE);
		long long int consumed = sizeof(arr[i][0]) * EAT_SIZE;
		total += consumed;
		printf("total consumed: %.2lf MB\n", (1.0*total)/(1<<20));
		if(i > 12630) sleep(20);
	}
	return;
}
void main(int argc, char const *argv[]) {
	hungry();
	return;
}
