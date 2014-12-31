#include "xhw3.h"
#include <signal.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <string.h>
#define NETLINK_USER 31
#define MAX_PAYLOAD 1024 /* maximum payload size*/

#define __NR_xjob	349	/* our private syscall number */
#define SIG_TEST	44	/* we define our own signal, hard coded. */
#define TIMEOUT		10	/* Default timeout in signal wait mode. */

int flag = 1;
static int echo_kernel(char *in_msg, char *out_msg);

void get_signal(int n, siginfo_t *info, void *unused)
{
	printf("Got signal from kernel: Job finished.\n");
	flag = 0;
}

void set_signal_handler()
{
	/* Setup signal handler. */
	struct sigaction sig;
	sig.sa_sigaction = get_signal;
	sig.sa_flags = SA_SIGINFO;
	sigaction(SIG_TEST, &sig, NULL);
}

void help()
{
	printf("Help current not available.\n");
}

int main(int argc, char *argv[])
{
	setlocale(LC_ALL, "");

	int result = 0, nof = 0;
	struct checksum_args chk_args;
	struct job job1;
	int sflag = 0, wflag = 0;
	char **inputfiles;
	char ch;
	char actualpath[PATH_MAX + 1];
	int timeout = TIMEOUT;
	int i = 0;
	int j = 0;
	char *out_msg = NULL, *in_msg = NULL, *temp = NULL;

	while ((ch = getopt(argc, argv, ":hwt:s")) != -1)
		switch (ch) {
		case 'h':	/* Help */
			help();
			exit(1);
			break;
		case 'w':       /* Signal mode */
			wflag = 1;
			sflag = 1;
			break;
		case 's':
			sflag = 1;
			break;
		case 't':       /* timeout*/
			timeout = atoi(optarg);
			if (timeout <= 0 || timeout > 50) {
				printf("Invalid timeout passed.\n"
				"Pass a number between 1 and 50\n");
				exit(1);
			}
			break;
		case '?':       /* Error */
			if (optopt == 't')
				fprintf(stderr,
				"Option -%c requires an argument.\n", optopt);
			else if (isprint(optopt))
				fprintf(stderr,
				"Unknown option `-%c'.\n", optopt);
			else
				fprintf(stderr,
				"Unknown option character `\\x%x'.\n", optopt);
				return 1;
		default:        /* Usage */
			(void)fprintf(stderr,
			"usage: ./user_encrypt [-hs] [-t timeout]"
			"infile1 infile2 ...\n");
			exit(1);
			/* NOTREACHED */
		}

	/* Checking if the output and input file names are passed. */
	if (argc-optind < 1) {
		fprintf(stderr,
		"ERROR: One or more input files "
		"to be passed as arguments. Aborting\n");
		exit(1);
	}

	/* Pointing to the first arguement(file). */
	argc -= (optind - 1);

	/* Calculating full paths. */
	nof = argc - 1;
	inputfiles = malloc(sizeof(char *) * nof);
	i = optind;
	j = 0;
	while (nof--) {
		strcpy(actualpath, "");
		realpath(argv[i], actualpath);
		inputfiles[j] = malloc((PATH_MAX + 1) * sizeof(char));
		strcpy(inputfiles[j], "");
		strcpy(inputfiles[j], actualpath);
		j++;
		i++;
	}

	/* Job init. */
	job1.type = USER;
	job1.operation = OP_CHECKSUM;

	/* Create argument for checksum. */
	chk_args.numfiles = argc - 1;
	chk_args.files = (const char **) inputfiles;
	chk_args.pid = getpid();
	chk_args.sigflag = wflag;
	chk_args.saveflag = sflag;

	job1.args = &chk_args;
	job1.argslen = sizeof(chk_args);

	if (wflag == 1)
		set_signal_handler();

	/* Making the syscall to post the job. */
	result = syscall(__NR_xjob, (void *) &job1, sizeof(job1));

	/* Parsing the result. */
	if (result < 0) {
		perror("ERROR");
		goto out;
	} else
		printf("Unique Job id = %d\n", result);

	if (wflag != 1)
		goto out;

	while ((flag == 1) && timeout--)
		sleep(1);

	if (flag) {
		printf("Timed out.\n");
		goto out;
	}

	/* Got the result from kernel now reading from socket */
	in_msg = malloc(10);
	memset(in_msg, 0, 10);
	temp = malloc(10);
	strcpy(in_msg, "");
	sprintf(temp, "%d", result);
	strcat(in_msg, temp);
	out_msg = malloc(MAX_PAYLOAD);
	memset(out_msg, 0, MAX_PAYLOAD);
	strcpy(out_msg, "");
	result = echo_kernel(in_msg, out_msg);
	printf("Checksum Result = "
	"Queue_id;Pid;Length of the result;Result\n%s\n", out_msg);

out:
	free(in_msg);
	free(out_msg);
	nof = chk_args.numfiles;
	j = 0;
	while (nof--)
		free(inputfiles[j++]);
	free(inputfiles);

	return result;
}


static int echo_kernel(char *in_msg, char *out_msg)
{
	struct sockaddr_nl src_addr, dest_addr;
	struct nlmsghdr *nlh = NULL;
	struct iovec iov;
	int sock_fd;
	struct msghdr msg;
	char *temp = malloc(MAX_PAYLOAD);
	sock_fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_USER);
	if (sock_fd < 0)
		return -1;

	memset(&src_addr, 0, sizeof(src_addr));
	src_addr.nl_family = AF_NETLINK;
	src_addr.nl_pid = getpid(); /* self pid */

	bind(sock_fd, (struct sockaddr *)&src_addr, sizeof(src_addr));

	memset(&dest_addr, 0, sizeof(dest_addr));
	memset(&dest_addr, 0, sizeof(dest_addr));
	dest_addr.nl_family = AF_NETLINK;
	dest_addr.nl_pid = 0; /* For Linux Kernel */
	dest_addr.nl_groups = 0; /* unicast */

	nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(MAX_PAYLOAD));
	memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD));
	nlh->nlmsg_len = NLMSG_SPACE(MAX_PAYLOAD);
	nlh->nlmsg_pid = getpid();
	nlh->nlmsg_flags = 0;

	strcpy(NLMSG_DATA(nlh), in_msg);

	iov.iov_base = (void *)nlh;
	iov.iov_len = nlh->nlmsg_len;
	msg.msg_name = (void *)&dest_addr;
	msg.msg_namelen = sizeof(dest_addr);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	printf("Sending result request to kernel\n");
	sendmsg(sock_fd, &msg, 0);
	printf("Waiting for response from kernel\n");

	/* Read message from kernel */

	recvmsg(sock_fd, &msg, 0);
	temp = (char *)NLMSG_DATA(nlh);
	strcpy(out_msg, temp);
	memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD));
	return 0;
}

