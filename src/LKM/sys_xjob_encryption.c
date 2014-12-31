#include "sys_xjob.h"

#define ENCSFX	".encf"
#define DCSFX	".dcf"
#define ENCSFXLEN	5
#define DCSFXLEN	4
#define ALG_CCMP_KEY_LEN	16
#define CRYPTO_ALG_ASYNC	0x00000080
#define SHA1_LENGTH	20
#define AES_KEY_SIZE	32
#define AES_BLOCK_SIZE	16
#define AES_HALF_KEY_SIZE	16
#define SALT_L	"Z2232Y3998X2230"
#define SALT_R	"Z8363E0932E8736"
#define MIN_PASS_LEN	8

static int generate_key(char *pass, u8 *pkey)
{
	int i;
	int err = 0;

	struct scatterlist sg;
	struct scatterlist sg1;
	struct crypto_hash *tfm;
	struct hash_desc desc;

	unsigned char output[SHA1_LENGTH];
	unsigned char output1[SHA1_LENGTH];
	unsigned char outputfull[AES_KEY_SIZE];

	char *buf1 = kmalloc(AES_HALF_KEY_SIZE, GFP_KERNEL);
	char *buf2 = kmalloc(AES_HALF_KEY_SIZE, GFP_KERNEL);
	int saltlen = AES_HALF_KEY_SIZE - strlen(pass);

	if (strlen(pass) > AES_HALF_KEY_SIZE) {
		strncpy(buf1, pass, AES_HALF_KEY_SIZE);
		strncpy(buf2, pass, AES_HALF_KEY_SIZE);
	} else {
		strncpy(buf1, pass, strlen(pass));
		strncpy(buf2, pass, strlen(pass));
		if (saltlen > 0) {
			strncpy(buf1, SALT_L, saltlen);
			strncpy(buf2, SALT_R, saltlen);
		}
	}

	memset(output, 0x00, SHA1_LENGTH);
	memset(output1, 0x00, SHA1_LENGTH);

	tfm = crypto_alloc_hash("sha1", 0, CRYPTO_ALG_ASYNC);

	if (IS_ERR(tfm)) {
		err = -1;
		goto out;
	}

	desc.tfm = tfm;
	desc.flags = 0;

	sg_init_one(&sg, buf1, AES_HALF_KEY_SIZE);
	sg_init_one(&sg1, buf2, AES_HALF_KEY_SIZE);
	crypto_hash_init(&desc);

	crypto_hash_update(&desc, &sg, AES_HALF_KEY_SIZE);
	crypto_hash_final(&desc, output);

	crypto_hash_update(&desc, &sg1, AES_HALF_KEY_SIZE);
	crypto_hash_final(&desc, output1);

	strncpy(outputfull, output, AES_HALF_KEY_SIZE);
	strncpy(outputfull+AES_HALF_KEY_SIZE, output1, AES_HALF_KEY_SIZE);

	for (i = 0; i < AES_KEY_SIZE; i++)
		pkey[i] = outputfull[i];

out:
	if (!IS_ERR(tfm))
		crypto_free_hash(tfm);

	kfree(buf1);
	kfree(buf2);

	return err;
}

static int file_encryption(char *filename, int edflag, char *pass)
{
	int err = 0;			/* Error varible. */
	mm_segment_t orgfs = get_fs();	/* Memory segment. */
	unsigned long page = 0;		/* The page from the kernel. */
	unsigned long page2 = 0;	/* The page from the kernel. */
	char *data;			/* Kernel page. */
	char *data2;			/* Kernel page. */
	struct file *fin = NULL;	/* Input file pointer. */
	struct file *fout = NULL;	/* Input file pointer. */
	int nbtrd;			/* nbtrd is Number of ByTes ReaD. */
	int nbtwt;			/* nbtwt is Number of ByTes WriTten. */
	char *out_file;			/* Output filename. */
	int countBytes = 0;		/* Number of bytes encoded/decoded. */
	int test = 0;			/* Test flag. */
	int start = 0;			/* Start flag. */
	struct crypto_cipher *tfm = NULL;	/* Crypto cipher. */
	u8 key[AES_KEY_SIZE];	/* Key. */

	/* Validating the password. */
	if (!pass || strlen(pass) < MIN_PASS_LEN) {
		err = -EINVAL;
		goto out;
	}

	/* Generate key from the password. */
	err = generate_key(pass, key);
	if (err)
		goto out;

	/* Crypto init. */
	tfm = crypto_alloc_cipher("aes", 0, CRYPTO_ALG_ASYNC);
	if (IS_ERR(tfm)) {
		printk(KERN_CRIT "Unable to allocate cipher\n");
		err = -EPERM;
		goto out;
	}

	err = crypto_cipher_setkey(tfm, key, AES_KEY_SIZE);
	if (err)
		goto out;

	if (edflag == 1) {
		out_file = kmalloc(strlen(filename) + ENCSFXLEN  + 1,
				GFP_KERNEL);
		strcpy(out_file, filename);
		strcpy(out_file + strlen(filename), ENCSFX);
	} else {
		out_file = kmalloc(strlen(filename) + DCSFXLEN  + 1,
				GFP_KERNEL);
		strcpy(out_file, filename);
		strcpy(out_file + strlen(filename), DCSFX);
	}

	set_fs(KERNEL_DS);
	page = __get_free_page(GFP_KERNEL);
	if (page)
		data = (char *)page;
	else {
		err = -ENOMEM;
		goto out;
	}

	page2 = __get_free_page(GFP_KERNEL);
	if (page2)
		data2 = (char *)page2;
	else {
		err = -ENOMEM;
		goto out;
	}

	fin = filp_open(filename, O_RDONLY, 0);
	if (!fin || IS_ERR(fin)) {
		err = PTR_ERR(fin);
		goto out;
	}

	fout = filp_open(out_file, O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR);
	if (!fout || IS_ERR(fout)) {
		err = PTR_ERR(fout);
		goto out;
	}

	memset(data, 0, AES_BLOCK_SIZE);
	memset(data2, 0, AES_BLOCK_SIZE);

	do {
		/* Reading from input file. */
		nbtrd = fin->f_op->read(fin,
			data,
			AES_BLOCK_SIZE,
			&fin->f_pos);

		if (nbtrd > 0) {

			test = nbtrd;

			/* Lets encrypt/decrypt the block before
			writing to the output file. */
			if (edflag == 1) {
				crypto_cipher_encrypt_one(tfm, data2, data);
				nbtwt = fout->f_op->write(fout,
					data2,
					AES_BLOCK_SIZE,
					&fout->f_pos);
			} else {
				if (start >= 1 && test == AES_BLOCK_SIZE) {
					nbtwt = fout->f_op->write(fout,
						data2,
						AES_BLOCK_SIZE,
						&fout->f_pos);
				} else
					nbtwt = nbtrd;
				start++;
				crypto_cipher_decrypt_one(tfm, data2, data);
			}

			if (nbtrd == nbtwt)
				countBytes += nbtwt;
			else {
				/* Error while writing. */
				if (nbtwt < 0)
					err = nbtwt;
				break;	/* While loop. */
			}
		} else if (nbtrd < 0) {
			/* Error while reading. */
			err = nbtrd;
			break;	/* While loop. */
		} else
			break; /* End_of_file. */

	} while ((nbtrd > 0) && (nbtrd == nbtwt));

	if (test == 0)
		goto out;

	nbtwt = fout->f_op->write(fout,
			data2,
			test,
			&fout->f_pos);

out:
	/* Clean up process. */
	if (!IS_ERR(tfm))
		crypto_free_cipher(tfm);
	if (page)
		free_page(page);
	if (page2)
		free_page(page2);
	set_fs(orgfs);
	if (fin && !IS_ERR(fin) && file_count(fin))
		filp_close(fin, NULL);
	if (fout && !IS_ERR(fout) && file_count(fout))
		filp_close(fout, NULL);

	return err;
}

int do_encrypt(struct job *job, int id)
{
	int err = 0;
	int i;
	char *filename;
	char *key;
	int scount = 0;
	struct encrypt_args *earg;
	char *res = kmalloc(1024, GFP_KERNEL);
	char *temp = kmalloc(100, GFP_KERNEL);
	res = memset(res, 0, 1024);
	strcpy(res, "");

	earg = job->args;
	key = (char *) earg->key;
	for (i = 0; i < earg->numfiles; i++) {
		filename = (char *)earg->files[i];
		err = file_encryption(filename, earg->edflag, key);
		if (err)
			sprintf(temp,
				"%s, Failed with error %d",
				filename,
				err);
		else {
			sprintf(temp, "%s, Success", filename);
			scount++;
		}

		strcat(res, temp);
		strcat(res, "; ");
		strcpy(temp, "");
	}

	if (earg->rmflag == 1)
		printk(KERN_INFO
		"Encryption: rm flag set. File has to be deleted.\n");

	if (earg->saveflag == 1 || earg->sigflag == 1)
		add_result(id, earg->pid, res, strlen(res));

	if (earg->sigflag == 1) {
		err = __send_signal(scount, earg->pid);
		if (err < 0)
			printk(KERN_CRIT "Error sending signal.\n");
	}

	kfree(temp);
	kfree(res);
	return err;
}

int encryption_operation(struct job *ja, struct job *j)
{
	int i, err = -EINVAL;
	char **filenames;
	struct encrypt_args *earg;
	struct encrypt_args *ja_earg;

	if (
		(ja->argslen != sizeof(struct encrypt_args *)) &&
		(!access_ok(VERIFY_READ, ja->args, ja->argslen))
	) {
		kfree(j);
		err = -EINVAL;
		goto out;
	}

	ja_earg = ja->args;
	earg = kmalloc(sizeof(struct encrypt_args), GFP_KERNEL);
	filenames = kmalloc(sizeof(char *)*ja_earg->numfiles, GFP_KERNEL);

	for (i = 0; i < ja_earg->numfiles; i++)
		if (access_ok(VERIFY_READ,
			ja_earg->files[i],
			sizeof(const char *))
		)
			filenames[i] = getname(ja_earg->files[i]);

	earg->files = (const char **)filenames;
	earg->numfiles = ja_earg->numfiles;
	earg->pid = ja_earg->pid;
	earg->sigflag = ja_earg->sigflag;
	earg->saveflag = ja_earg->saveflag;
	earg->rmflag = ja_earg->rmflag;
	earg->edflag = ja_earg->edflag;
	earg->key = getname(ja_earg->key);

	j->args = earg;
	err = produce(j);

out:
	return err;
}
