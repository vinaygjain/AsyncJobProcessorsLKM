#include "xhw3.h"
#include <signal.h>

#define __NR_xjob	349	/* our private syscall number */
#define SIG_TEST	44	/* we define our own signal, hard coded. */
#define TIMEOUT		10	/* Default timeout in signal wait mode. */
#define RESULT_SIZE	300	/* Size of the result buffer. */
#define PATH_MAX	1024

int flag;
int resp;

void get_signal(int n, siginfo_t *info, void *unused)
{
	resp = (int)info->si_int;
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

	int result = 0, i = 0, j = 0, nof = 0;
	struct encrypt_args enc_args;
	struct job job1;
	int sflag = 0;
	int wflag = 0;
	int rflag = 0;
	char ch;
	char *key = NULL;
	char **files = NULL;
	char actualpath[PATH_MAX + 1];
	int timeout = TIMEOUT;
	int ed_flag = 0;

	/* Flag init. */
	flag = 1;
	resp = 0;

	while ((ch = getopt(argc, argv, ":herwdt:sk:")) != -1)
		switch (ch) {
		case 'h':	/* Help */
			help();
			exit(1);
			break;
		case 'w':       /* Signal/wait mode */
			wflag = 1;
			break;
		case 's':       /* Save result flag */
			sflag = 1;
			break;
		case 'r':       /* Remove file flag */
			rflag = 1;
			break;
		case 'd':       /* Encryption mode */
			ed_flag = 2;
			break;
		case 'e':       /* Decryption mode */
			ed_flag = 1;
			break;
		case 'k':       /* Timeout */
			key = optarg;
			break;
		case 't':       /* Timeout */
			timeout = atoi(optarg);
			if (timeout <= 0 || timeout > 50) {
				printf("Invalid timeout passed.\n"
				"Pass a number between 1 and 50\n");
				exit(1);
			}
			break;
		case '?':       /* Error */
			if (optopt == 't' || optopt == 'k')
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
			"usage: ./user_encrypt [-hs] [-d -e] [-k KEY] [-t timeout]"
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

	if (!ed_flag) {
		printf("Encryption or Decryption flag has to be passed."
			" Exiting.\n");
		exit(1);
	}

	if (!key) {
		printf("Key is required. Exiting.\n");
		exit(1);
	}

	/* Pointing to the first arguement. */
	argv += optind-1;

	job1.type = USER;
	job1.operation = OP_ENCRYPT;

	/* Remove the program name and adjust argc. */
	argv  += 1;
	argc -= (optind - 1);

	/* Create argument for encryption. */
	files = malloc(sizeof(char *) * nof);
	nof = argc - 1;
	i = 0;
	j = 0;
	while (nof--) {
		realpath(argv[i++], actualpath);
		files[j] = malloc((PATH_MAX + 1) * sizeof(char));
		strcpy(files[j], "");
		strcpy(files[j++], actualpath);
	}

	enc_args.numfiles = argc - 1;
	enc_args.files = (const char **)files;
	enc_args.pid = getpid();
	enc_args.sigflag = wflag;
	enc_args.saveflag = sflag;
	enc_args.rmflag = rflag;
	enc_args.edflag = ed_flag;
	enc_args.key = key;

	job1.args = &enc_args;
	job1.argslen = sizeof(enc_args);

	if (wflag)
		set_signal_handler();

	/* Making the syscall to post the job. */
	result = syscall(__NR_xjob, (void *) &job1, sizeof(job1));

	/* Parsing the result. */
	if (result < 0) {
		perror("ERROR");
		goto out;
	} else
		printf("Job posted. Unique Job id = %d\n", result);

	/* If signal mode not set, exit. */
	if (!wflag)
		goto out;

	printf("Waiting for the response.\n");
	while (flag && timeout--)
		sleep(1);

	if (!flag)
		printf("Number of files successfully"
		" encrypted/decrypted is %d\n", resp);
	else
		printf("Timed out. Exiting.\n");

out:
	return result;
}
