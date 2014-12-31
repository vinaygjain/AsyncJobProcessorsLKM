#include "xhw3.h"

#define __NR_xjob	349	/* our private syscall number */

int main(int argc, char *argv[])
{
	int result = 0;
	struct compress_args comp_args;
	struct job job1;

	job1.type = USER;
	job1.operation = OP_COMPRESS;

	/* Skip the program name. */
        argv += 1;

	/* Create argument for checksum. */
	comp_args.files = (const char **)argv;
	comp_args.numfiles = argc - 1;

	job1.args = &comp_args;
	job1.argslen = sizeof(comp_args);

	/* Making the syscall to post the job. */
	result = syscall(__NR_xjob, (void*) &job1, sizeof(job1));

	/* Parsing the result. */
	if (result < 0)
		perror("ERROR");
	else
		printf("Unique Job id = %d\n",result);
		
	return result;
}
