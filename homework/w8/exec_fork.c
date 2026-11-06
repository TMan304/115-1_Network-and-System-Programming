#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

int main()
{
	pid_t pid;
	int n;

	printf("fork program starting\n");
	pid = fork();
	switch(pid)
	{
		case -1:
			perror("fork failed\n");
			exit(1);
		case 0:
			execlp("ps", "ps", "ax", NULL);
			break;
		default:
			printf("Parent here\n");
			usleep(5*1000000);
			printf("Done Sleeping\n");
			break;
	}
	exit(0);
}
