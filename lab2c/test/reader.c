#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>


#define CIJEV	"/dev/shofer"
#define MAXSZ	16

int main(int argc, char *argv[])
{
	int fp;
	char buffer[MAXSZ];
	size_t size;
	long pid = (long) getpid();

	fp = open(CIJEV, O_RDONLY);
	if (fp == -1) {
		perror("Nisam otvorio cjevovod! Greska: ");
		return -1;
	}

	while(1) {
		memset(buffer, 0, MAXSZ);
		printf("Citac %ld poziva read\n", pid);
		size = read(fp, buffer, MAXSZ);
		if (size == -1) {
			perror("Greska pri citanju! Greska: ");
			break;
		}
		printf("Citac %ld procitao (%ld): %s\n", pid, size, buffer);
		sleep(1);
	}

	return -1;
}