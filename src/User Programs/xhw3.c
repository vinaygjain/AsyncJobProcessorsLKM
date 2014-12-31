#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <locale.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include "xhw3.h"

#define __NR_xjob	349	/* our private syscall number */
#define S_RUNNING 1
#define S_WAITING 2
#define S_COMPLETE 3
#define ADMIN 1
#define USER 2
#define list 1
#define remove 2
#define checksum 1
#define encrypt 2
#define compress 3

int main(int argc, char *argv[])
{
	int result = 0;
	struct checksum_args chk_args;
	/* Allocate memory for main job structure. */
	struct job job1;

	job1.type = atoi(argv[1]);
	job1.operation = atoi(argv[2]);
	argv += 3; /* point to arg */
	if (job1.type == USER) {
		switch (job1.operation) {
		case checksum:
			chk_args.files = (const char **)argv;
			chk_args.numfiles = argc - 3;
			job1.args = &chk_args;
			job1.argslen = sizeof(chk_args);

			/* Making the syscall to post the job. */
			result = syscall(__NR_xjob,
					(void *) &job1,
					sizeof(job1)
					);

			/* Parsing the result. */
			if (result < 0)
				perror("ERROR");
			else
				printf("Unique Job id = %d\n", result);

			return result;
		}
	}
	/* Job type is admin */
	if (job1.type == ADMIN) {
		struct common_args c_args;
		c_args.id = atoi(argv[3]);
		job1.args = &c_args;
		job1.argslen = sizeof(c_args);

		result = syscall(__NR_xjob, (void *) &job1, sizeof(job1));

		/* Parsing the result. */
		if (result < 0)
			perror("ERROR");
		else {


			switch (result) {

			case S_RUNNING:
			printf("Status Running\n");
			break;

			case S_WAITING:
			printf("Status Waiting\n");
			break;

			case S_COMPLETE:
			printf("Status Waiting\n");
			break;

			}
		}
	}

	return result;
}
