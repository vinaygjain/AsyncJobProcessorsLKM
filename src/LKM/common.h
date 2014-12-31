
struct job {
	unsigned int type;	/* Type :  Admin or user. */
	unsigned int operation;	/* Operation to be performed. */
	void *args;		/* Argument pointer. */
	unsigned int argslen;	/* Size of arguments. */
};

struct checksum_args {
	const char **files;	/* Input file names. */
	int numfiles;		/* Number of input files. */
	int pid;		/* PID of the process submitting the job. */
	int sigflag;		/* Wait/Signal flag. */
	int saveflag;		/* Save the result in the queue. */
};

struct encrypt_args {
	const char **files;     /* Input file names */
	int numfiles;           /* Number of input files. */
	int pid;		/* PID of the process submitting the job. */
	int sigflag;		/* Wait/Signal flag. */
	int saveflag;		/* Save the result in the queue. */
	int rmflag;		/* Remove the input file after encryption. */
	int edflag;		/* Encrption/Decryption flag. */
	const char *key;	/* Password to be used. */
};

struct compress_args {
	const char **files;     /* Input file names */
	int numfiles;           /* Number of input files. */
};

struct common_args {
	unsigned id;		/* Queue ID of submitted job. */
	void *result;		/* Result. */
	unsigned int res_len;	/* Size of the result. */
};
