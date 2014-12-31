#include "xhw3.h"
#include <sys/socket.h>
#include <linux/netlink.h>
#include <string.h>
#define NETLINK_USER 31
#define MAX_PAYLOAD 1024 /* maximum payload size*/
#define OP_LIST 1
#define OP_REMOVE 2
#define __NR_xjob	349	/* our private syscall number */

struct sockaddr_nl src_addr, dest_addr;
struct nlmsghdr *nlh;
struct iovec iov;
int sock_fd;
struct msghdr msg;
static int admin_netlink(char *send_msg, char *recv_msg);

void help()
{
	printf("Help currently not available.\n");
}

int main(int argc, char *argv[])
{
	int result = 0, use_soc = 0;
	int type = 0;
	unsigned int id = 0;
	struct job job1;
	struct common_args c_args;
	c_args.result = NULL;
	char ch;
	char *send_msg, *temp, *recv_msg;

	while ((ch = getopt(argc, argv, "hl:r:sj:")) != -1)
		switch (ch) {

		case 'h':	/* Help */
			help();
			exit(1);
			break;
		case 'l':       /* list operation */
			if (strcmp(optarg, "all") == 0) {
				use_soc = 1;
				send_msg = malloc(100);
				temp = malloc(100);
				strcpy(send_msg, "");
				sprintf(temp, "list:0");
				strcat(send_msg, temp);
			} else
				id = atoi(optarg);
			/* list only single job */
				job1.type = ADMIN;
				job1.operation = OP_LIST;
				type = OP_LIST;

			if (id < 0) {
				printf("Invalid id passed.\n"
				"Pass a positive integer\n");
				exit(1);
			}
			break;
		case 'r':       /* remove operation */
			if (strcmp(optarg, "all") == 0)
				id = 0;
			else
				id = atoi(optarg);
			job1.type = ADMIN;
			job1.operation = OP_REMOVE;
			type = OP_REMOVE;
			if (id < 0) {
				printf("Invalid id passed.\n"
				"Pass a positive integer\n");
				exit(1);
			}
			break;
		case 'j': /* List all results */
			use_soc = 1;
			type = OP_RESULT;
			send_msg = malloc(100);
			temp = malloc(100);
			strcpy(send_msg, "");
			if (strcmp(optarg, "all") == 0) {
				sprintf(temp, "res:0");
			} else {
				id = atoi(optarg);
				sprintf(temp, "%d", id);
			}
			strcat(send_msg, temp);
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
				"usage: ./admin [-l id] [-r id]");
			exit(1);
			/* NOTREACHED */
	}

	/* Pointing to the first arguement. */
	argv += optind-1;
	c_args.id = id;

	if (use_soc) {
		/* use socket to get list of jobs */
		recv_msg = malloc(MAX_PAYLOAD);
		admin_netlink(send_msg, recv_msg);
		if (type == OP_RESULT)
			printf(
			"Queue_id;Pid;Length of the result;Result\n%s\n",
			recv_msg);
		else if (type == OP_LIST)
			printf(
			"List of jobs waiting in thw queue are\n%s\n",
			recv_msg);
		else
			printf("UNKNOWN RESULT:\n%s\n", recv_msg);
		goto out;
	}

	/* Use syscall interface */
	job1.args = &c_args;
	job1.argslen = sizeof(c_args);

	result = syscall(__NR_xjob, (void *) &job1, sizeof(job1));
	/* Parsing the result. */
	if (result < 0) {
		printf("Job %d - doesn't exists\n", id);
		goto out;
	}

	/* read status of single job id */
	if (type == OP_LIST)
		switch (result) {
		case S_RUNNING:
			printf("Status Running\n");
			break;
		case S_WAITING:
			printf("Status Waiting\n");
			break;
		case S_COMPLETE:
			printf("Status Complete\n");
			break;
		default:
			printf("Unknown status passed.\n");
		}
	else if (type == OP_REMOVE) {
		if (id == 0 && result > 0)
			printf(
			"%d jobs successfully removed.\n",
			result);
		else if (id == 0 && result == 0)
			printf(
			"Work queue empty. No jobs removed.\n");
		else if (id == result)
			printf(
			"Job: %d - removed successfully.\n",
			id);
		else
			printf("UNKNOWN RESULT\n");
	} else
		printf("Error parsing request.\n");

out:
	if (!c_args.result)
		free(c_args.result);
	if (!temp)
		free(temp);
	if (!send_msg)
		free(send_msg);
	if (!recv_msg)
		free(recv_msg);
	return result;
}

static int admin_netlink(char *send_msg, char *recv_msg)
{
	int err = 0;
	char *temp = malloc(MAX_PAYLOAD);
	nlh = NULL;

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

	strcpy(NLMSG_DATA(nlh), send_msg);

	iov.iov_base = (void *)nlh;
	iov.iov_len = nlh->nlmsg_len;
	msg.msg_name = (void *)&dest_addr;
	msg.msg_namelen = sizeof(dest_addr);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	printf("Sending request to kernel\n");
	sendmsg(sock_fd, &msg, 0);
	printf("Waiting for response from kernel\n");

	/* Read message from kernel */
	recvmsg(sock_fd, &msg, 0);
	temp = (char *)NLMSG_DATA(nlh);
	strcpy(recv_msg, temp);
	memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD));
	return err;
}

