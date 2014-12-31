#include "sys_xjob.h"

static u32 file_checksum(char *filename)
{
	u32 crc = ~0;			/* Checksum. */
	mm_segment_t orgfs = get_fs();	/* Memory segment. */
	unsigned long page = 0;		/* The page from the kernel. */
	char *data;			/* Kernel page. */
	struct file *fin = NULL;	/* Input file pointer. */
	int nbtrd;			/* nbtrd is Number of ByTes ReaD. */

	set_fs(KERNEL_DS);
	page = __get_free_page(GFP_KERNEL);
	if (page)
		data = (char *)page;
	else
		goto out;

	fin = filp_open(filename, O_RDONLY, 0);
	if (!fin || IS_ERR(fin)) {
		crc = -ENOENT;
		goto out;
	}

	do {
		/* Reading from input file. */
		nbtrd = fin->f_op->read(fin,
					data,
					16,
					&fin->f_pos);
		if (nbtrd < 0) {
			goto out;
			crc = -EIO;
		} else
			crc = crc32c(crc, data, nbtrd);
	} while (nbtrd > 0);

out:
	/* Clean up process. */
	if (page)
		free_page(page);
	set_fs(orgfs);
	if (fin && !IS_ERR(fin) && file_count(fin))
		filp_close(fin, NULL);

	return crc;
}

int write_result_to_file(int id, char *buf)
{
	char *filename = kmalloc(100, GFP_KERNEL);
	char *temp = kmalloc(50, GFP_KERNEL);
	struct file *fp = NULL;
	int err = 0;
	int bytes = 0;

	sprintf(temp, "/tmp/results/%d.chksum", id);
	strcpy(filename, "");
	strcat(filename, temp);

	fp = filp_open(filename, O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR);
	if (!fp || IS_ERR(fp)) {
		err = PTR_ERR(fp);
		printk(KERN_CRIT "FAILED 2 wite error is %d", err);
		goto out;
	}
	bytes = fp->f_op->write(fp, buf, strlen(buf), &fp->f_pos);

	if (bytes != strlen(buf)) {
		err = -EIO;
		goto out;
	}

out:
	if (fp && !IS_ERR(fp) && file_count(fp))
		filp_close(fp, NULL);
	kfree(temp);
	kfree(filename);
	return err;
}

int do_checksum(struct job *job, int id)
{
	int err = 1;
	int i;
	u32 chk = 0;
	struct checksum_args *carg;
	char *filename = NULL;
	char *res = kmalloc(1024, GFP_KERNEL);
	char *temp = kmalloc(100, GFP_KERNEL);

	res = memset(res, 0, 1024);
	temp = memset(temp, 0, 100);
	strcpy(res, "");

	carg = job->args;
	for (i = 0; i < carg->numfiles; i++) {
		filename = (char *)carg->files[i];
		chk = file_checksum(filename);
		if (chk == -ENOENT)
			sprintf(temp, "%s, NO SUCH FILE.", filename);
		else if (chk == -EIO)
			sprintf(temp, "%s, ERROR READING FILE.", filename);
		else if (chk == 0)
			sprintf(temp, "%s, UNKNOWN ERROR.", filename);
		else
			sprintf(temp, "%s, %u", filename, chk);

		strcat(res, temp);
		strcat(res, "; ");
		strcpy(temp, "");
	}

	if (carg->saveflag == 1 || carg->sigflag == 1)
		add_result(id, carg->pid, res, strlen(res));


	if (carg->sigflag != 1) {
		/* user choose not to wait for result */
		printk("chksum in write to file mode\n");
	err = write_result_to_file(id, res);
	if (err)
		printk(KERN_CRIT "FAILED TO WRITE\n");
		goto out;
	}

	err = __send_signal(500, carg->pid);
	if (err < 0) {
		printk(KERN_CRIT "Error sending signal. Writing result to file.\n");
		printk(KERN_INFO "Writing checksum result to file\n");
		err = write_result_to_file(id, res);
		goto out;
	}

out:
	kfree(temp);
	kfree(res);
	return err;
}

int checksum_operation(struct job *ja, struct job *j)
{
	int i, err;
	char **filenames;
	struct checksum_args *earg;
	struct checksum_args *ja_earg;

	if ((ja->argslen != sizeof(struct checksum_args *)) &&
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

	j->args = earg;
	err = produce(j);

out:
	return err;
}
