#include "sys_xjob.h"
#define RESULTS_MAX	300

struct sock *nl_sk;	/* Socket for netlink. */

/* Consumer threads. */
static struct task_struct *c_thread1;	/* Consumer thread1 */
static struct task_struct *c_thread2;	/* Consumer thread2 */
static struct task_struct *c_thread3;	/* Consumer thread3 */
char t_name1[] = "ct1";			/* First consumer thread name */
char t_name2[] = "ct2";			/* Second consumer thread name */
char t_name3[] = "ct3";			/* Third consumer thread name */

/* Waitqueues and lock. */
wait_queue_head_t cwaitq;		/* Queue on which consumer will wait */
wait_queue_head_t pwaitq;		/* Queue on which producer will wait */
static DEFINE_MUTEX(wq);

/* The work queue structure for holding the jobs to be processed. */
struct work {
	int queue_id;		/* Unique queue_ID to differentiate jobs. */
	struct job *job;	/* Pointer to the job structure. */
	struct work *next;	/* Pointer to the next queue item. */
};

struct work *head;		/* Head of the work queue. */
struct work *tail;		/* Tail of the work queue. */
static int qlen;		/* Current queue length */
static int jobID_count = 4000;	/* Unique jobID return to the user */

/* The result queue structure for holding the results of the processed jobs. */
struct result {
	int queue_id;		/* Work queue ID of the processed job. */
	int pid;		/* PID of process submitting the job. */
	char *res;		/* Result of the operation(String). */
	int reslen;		/* Length of the result. */
	struct result *next;	/* Pointer to the next result. */
};

struct result *rhead;	/* Head of the result queue. */
struct result *rtail;	/* Tail of the result queue. */
static int rlen;	/* Current result queue length */

/* To free the checksum object. */
static void __free_chk(struct checksum_args *chk)
{
	int i;
	for (i = 0; i < chk->numfiles; i++)
		if (chk->files[i] != NULL)
			putname(chk->files[i]);

	kfree(chk->files);
	kfree(chk);
	return;
}

/* To free the encryption object. */
static void __free_enc(struct encrypt_args *enc)
{
	int i;
	for (i = 0; i < enc->numfiles; i++)
		if (enc->files[i] != NULL)
			putname(enc->files[i]);

	putname(enc->key);
	kfree(enc->files);
	kfree(enc);
	return;
}

/* To free the result. */
static void __kfree_result(struct result *r)
{
	kfree(r->res);
	kfree(r);
}

/* To free the work queue item. */
static void __kfree_work(struct work *w)
{
	int op = 0, sub_op = 0;
	op = w->job->type;
	sub_op = w->job->operation;

	switch (op) {

	case ADMIN:
			switch (sub_op) {
			case OP_REMOVE:
			break;
			}
			break;

	case USER:
		switch (sub_op) {
		case OP_CHECKSUM:
			__free_chk(w->job->args);
			break;
		case OP_ENCRYPT:
			__free_enc(w->job->args);
			break;
		default:
			printk(KERN_CRIT "Unknown job for kfree\n");
			return;
		}
		break;

	}

	kfree(w->job);
	kfree(w);
}

void add_result(int qid, int pid, char *res, int len)
{
	struct result *r;

	mutex_lock(&wq);
	if (rlen >= RESULTS_MAX) {
		r = rhead;
		rhead = r->next;
		rlen--;
		__kfree_result(r);
	}

	r = kmalloc(sizeof(struct result), GFP_KERNEL);
	if (r == NULL)
		goto out;

	r->res = kmalloc(len, GFP_KERNEL);
	if (r->res == NULL) {
		kfree(r);
		goto out;
	}

	strncpy(r->res, res, len);
	rlen++;
	r->queue_id = qid;
	r->reslen = len;
	r->pid = pid;
	r->next = NULL;

	if (rlen == 1)
		rhead = r;
	else
		rtail->next = r;

	rtail = r;
out:
	mutex_unlock(&wq);
}

int produce(struct job *j)
{
	int err = 0;
	int retry = 0;
	struct work *work;

	if (j == NULL)
		return -EINVAL;

	mutex_lock(&wq);
	if (qlen >= QMAX) {
		printk(KERN_DEBUG "Producer: Q full.\n");
		do {
			mutex_unlock(&wq);
			retry++;
			wait_event_interruptible(pwaitq, qlen < QMAX);
			mutex_lock(&wq);
			if (qlen < QMAX)
				goto insert;
		} while (retry <= RETRY_MAX);
		err = -EBUSY;
		goto out;
	}

insert:
	work = kmalloc(sizeof(struct work), GFP_KERNEL);
	if (work == NULL) {
		err = -ENOMEM;
		goto out;
	}

	qlen++;
	work->queue_id = jobID_count++;
	work->job = j;
	work->next = NULL;

	if (qlen == 1)
		head = work;
	else
		tail->next = work;

	tail = work;
	err = work->queue_id;
	wake_up_interruptible(&cwaitq);

out:
	mutex_unlock(&wq);
	return err;
}

static int __list_one_job(int id)
{
	int err = -EINVAL;
	struct work *i;

	mutex_lock(&wq);
	if (qlen == 0) {
		if (id < jobID_count)
			err = S_COMPLETE;
	} else {
		if ((id <= tail->queue_id) && (id >= head->queue_id)) {
			i = head;
			while (i != NULL && i->queue_id != id)
				i = i->next;

			if (i != NULL && i->queue_id == id)
				err = S_WAITING;
		} else if (id <= (head->queue_id - 2))
			err = S_COMPLETE;
		else if (id == (head->queue_id - 1))
			err = S_RUNNING;
	}
	mutex_unlock(&wq);

	return err;
}

static int __list_all_jobs(void *result, int res_len)
{
	int err = 1;
	int i;
	struct work *w;

	mutex_lock(&wq);
	int res[qlen + 1];
	w = head;
	res[0] = qlen;

	for (i = 1; i <= qlen; i++) {
		res[i] = w->queue_id;
		w = w->next;
	}
	mutex_unlock(&wq);

	if (access_ok(VERIFY_WRITE, result, res_len)) {
		if (res_len < sizeof(res))
			err = copy_to_user(result, res, res_len);
		else
			err = copy_to_user(result, res, sizeof(res));
	} else
		err = -EBADF;

	return err;
}

char *enumerate_one_result(int id)
{
	int i;
	int count = 0;
	struct result *r;
	char *msg = NULL;
	char *temp = NULL;

	msg = kmalloc(800, GFP_KERNEL);
	temp = kmalloc(300, GFP_KERNEL);

	mutex_lock(&wq);
	r = rhead;
	strcpy(msg, "ONE::");
	memset(msg, 0, 800);
	memset(temp, 0, 300);

	for (i = 1; i <= rlen; i++) {
		if (r->queue_id == id) {
			sprintf(temp,
				"|%d;%d;%d;",
				r->queue_id,
				r->pid,
				r->reslen);
			strncpy(temp+strlen(temp), r->res, r->reslen);
			if ((count + strlen(temp)) < 290) {
				strncpy(msg+count, temp, strlen(temp));
				count += strlen(temp);
				strcpy(temp, "");
			}
			break;
		}
		r = r->next;
	}

	mutex_unlock(&wq);
	kfree(temp);
	return msg;
}

char *enumerate_results()
{
	int i;
	int count = 0;
	struct result *r;
	char *msg = NULL;
	char *temp = NULL;

	msg = kmalloc(800, GFP_KERNEL);
	temp = kmalloc(300, GFP_KERNEL);

	mutex_lock(&wq);
	r = rhead;
	strcpy(msg, "ALL::");
	memset(msg, 0, 800);
	memset(temp, 0, 300);

	for (i = 1; i <= rlen; i++) {
		sprintf(temp, "|%d;%d;%d;", r->queue_id, r->pid, r->reslen);
		strncpy(temp+strlen(temp), r->res, r->reslen);
		if ((count + strlen(temp)) < 290) {
			strncpy(msg+count, temp, strlen(temp));
			count += strlen(temp);
			strcpy(temp, "");
		}
		r = r->next;
	}

	mutex_unlock(&wq);
	kfree(temp);
	return msg;
}

char *enumerate_jobs()
{
	int i;
	struct work *w;
	char *msg = NULL;
	char *temp = NULL;

	temp = kmalloc(10 * sizeof(char), GFP_KERNEL);
	msg = kmalloc((2 * QMAX) * sizeof(int), GFP_KERNEL);

	mutex_lock(&wq);
	w = head;
	strcpy(msg, "");
	UDBG;

	for (i = 1; i <= qlen; i++) {
		sprintf(temp, "%d", w->queue_id);
		strcat(msg, temp);
		strcat(msg, " ");
		strcpy(temp, "");
		w = w->next;
	}
	strcat(msg, "-1");

	mutex_unlock(&wq);
	kfree(temp);
	return msg;
}

/* TODO number of jobs more than user buffer size
	then send in mutiple attempt. */
static int list_jobs(void *args)
{
	int err;
	struct common_args *carg;
	carg = args;

	if (carg->id != 0)
		err = __list_one_job(carg->id);
	else
		err = __list_all_jobs(carg->result, carg->res_len);

	return err;
}

static int __remove_one_job(int id)
{
	int err = -EINVAL;
	struct work *i, *p;

	mutex_lock(&wq);
	if ((qlen > 0) &&
		(id <= tail->queue_id) &&
		(id >= head->queue_id)
	) {
		i = head;
		p = NULL;
		while (i != NULL) {
			if (i->queue_id == id)
				break;
			p = i;
			i = i->next;
		}
		if (i != NULL && i->queue_id == id) {
			if (p == NULL)
				head = i->next;
			else
				p->next = i->next;

			err = id;
			__kfree_work(i);
			qlen--;
		}
	}
	mutex_unlock(&wq);

	return err;
}

static int __remove_all_jobs(void)
{
	int count = 0;
	struct work *i;

	mutex_lock(&wq);
	i = head;

	while (i != NULL) {
		head = i->next;
		__kfree_work(i);
		i = head;
		qlen--;
		count++;
	}
	mutex_unlock(&wq);

	return count;
}

static int remove_jobs(void *args)
{
	int err;
	struct common_args *carg;
	carg = args;

	if (carg->id != 0)
		err = __remove_one_job(carg->id);
	else
		err = __remove_all_jobs();

	return err;
}

static int __compress_operation(void)
{
	printk(KERN_DEBUG "Function %s called.\n", __func__);
	return 1000;
}

asmlinkage extern long (*sysptr)(void *arg, int argslen);

asmlinkage long xjob(void *arg, int argslen)
{
	int err;
	struct job *j;
	struct job *ja;

	ja = arg;
	if (!access_ok(VERIFY_READ, ja, argslen))
		return -EFAULT;

	if (argslen != sizeof(struct job))
		return -EINVAL;

	j = kmalloc(sizeof(struct job), GFP_KERNEL);

	if (copy_from_user(j, ja, argslen) != 0) {
		kfree(j);
		return -EFAULT;
	}

	if (j->type == ADMIN) {
		if (j->operation == OP_LIST) {
			/* List operation. */
			err = list_jobs(ja->args);
		} else if (j->operation == OP_REMOVE) {
			/* Remove operation. */
			err = remove_jobs(ja->args);
		} else
			err = -EINVAL;
			kfree(j);
		} else if (j->type == USER) {
			if (j->operation == OP_CHECKSUM) {
				/* Checksum operation. */
				err = checksum_operation(ja, j);
			} else if (j->operation == OP_ENCRYPT) {
				/* Encryption. */
				err = encryption_operation(ja, j);
			} else if (j->operation == OP_COMPRESS) {
				/* Compress. */
				err = __compress_operation();
			} else
				err = -EINVAL;
		} else
			err = -EINVAL;

	return err;
}

int consume(void *data)
{
	struct work *w = NULL;

	printk(KERN_INFO "Starting the consumer thread\n");
	while (!kthread_should_stop()) {

		/* wait till atleast one job is present in queue */
		wait_event_interruptible(cwaitq, qlen > 0);
		mutex_lock(&wq);
		/* Justify why checking for QMAX */
		if (qlen > 0 && qlen <= QMAX) {
			w = head;
			head = w->next;
			qlen--;
			/* If less than full */
			if (qlen == (QMAX - 1))
				wake_up_interruptible(&pwaitq);

		}
		mutex_unlock(&wq);

		if (w == NULL) {
			printk(KERN_CRIT
			"WEIRD: Out from wait and the job queue is empty.\n");
			printk(KERN_CRIT
			"WEIRD: May be exit thread was called.\n");
			msleep(200);
			continue;
		}

		printk(KERN_DEBUG "Start of work - %d\n", w->queue_id);
		if (w->job->type != 2)
			goto free;

		switch (w->job->operation) {
		case 1:
			if (do_checksum(w->job, w->queue_id))
				printk(KERN_DEBUG "Checksum success\n");
			else
				printk(KERN_DEBUG "Checksum fail\n");
			break;
		case 2:
			if (do_encrypt(w->job, w->queue_id) >= 0)
				printk(KERN_DEBUG "Encrypt success\n");
			else
				printk(KERN_DEBUG "Encrypt fail\n");
			break;
		default:
			printk(KERN_CRIT "Unknown job\n");
		}

free:
		printk(KERN_DEBUG "End of work - %d\n", w->queue_id);
		__kfree_work(w);
		w = NULL;
		msleep(200);
	}

	return 0;
}

static int __init init_sys_xjob(void)
{
	int err = 0;

	if (sysptr == NULL)
		sysptr = xjob;
	else
		goto out;

	printk(KERN_INFO "Installed a new sys_xjob module.\n");

	/* Init. */
	head = NULL;
	tail = NULL;
	rhead = NULL;
	rtail = NULL;
	nl_sk = NULL;
	qlen = 0;
	rlen = 0;
	init_waitqueue_head(&cwaitq);
	init_waitqueue_head(&pwaitq);

	/* Initialing the netlink communication. */
	nl_sk = netlink_kernel_create(&init_net,
					NETLINK_USER,
					0,
					hello_nl_recv_msg,
					NULL,
					THIS_MODULE);
	if (!nl_sk) {
		printk(KERN_ALERT "Error creating netlink socket.\n");
		err = -10;
		goto out;
	}
	printk(KERN_DEBUG "Netlink creation successful.\n");

	/* Creating a consumer thread. */
	c_thread1 = kthread_run(consume, NULL, t_name1);
	if (IS_ERR(c_thread1)) {
		printk(KERN_CRIT "First Thread creation failed.\n");
		err = -1;
		goto out;
	}
	printk(KERN_DEBUG "First consumer thread creation successful.\n");

	/* Creating a second consumer thread. */
	c_thread2 = kthread_run(consume, NULL, t_name2);
	if (IS_ERR(c_thread2)) {
		printk(KERN_CRIT "second Thread creation failed.\n");
		err = -1;
		goto out;
	}
	printk(KERN_DEBUG "Second consumer thread creation successful.\n");

	/* Creating a third consumer thread. */
	c_thread3 = kthread_run(consume, NULL, t_name3);
	if (IS_ERR(c_thread3)) {
		printk(KERN_CRIT "Third thread creation failed.\n");
		err = -1;
		goto out;
	}
	printk(KERN_DEBUG "Third consumer thread creation successful.\n");

out:
	return err;
}

static void  __exit exit_sys_xjob(void)
{
	struct work *i;
	struct result *j;

	mutex_lock(&wq);

	printk(KERN_DEBUG "Clearing the work queue.\n");
	i = head;
	while (i != NULL) {
		printk(KERN_DEBUG
			"The job type is %d and queue_id is %d\n",
			i->job->type, i->queue_id);
		head = i->next;
		__kfree_work(i);
		i = head;
		qlen--;
	}

	printk(KERN_DEBUG "Clearing the result queue.\n");
	j = rhead;
	while (j != NULL) {
		printk(KERN_DEBUG
			"Result of queue_id - %d and job_id - %d is\n%s\n",
			j->queue_id, j->pid, j->res);
		rhead = j->next;
		__kfree_result(j);
		j = rhead;
		rlen--;
	}

	/* TODO Justify why setting to QMAX */
	qlen = QMAX+1;
	mutex_unlock(&wq);

	/* stopping the consumer thread */
	printk(KERN_DEBUG "Waking the consumer thread for exit.\n"
	"Stopping the consumer threads.\n");
	wake_up_interruptible(&cwaitq); /* wake up consumer to stop it */
	kthread_stop(c_thread1);
	wake_up_interruptible(&cwaitq); /* wake up consumer to stop it */
	kthread_stop(c_thread2);
	wake_up_interruptible(&cwaitq); /* wake up consumer to stop it */
	kthread_stop(c_thread3);

	/* destroying socket */
	netlink_kernel_release(nl_sk);

	/* Setting sysptr to null. */
	if (sysptr != NULL)
		sysptr = NULL;
	printk(KERN_INFO "Removed sys_xjob module.\n");
}

module_init(init_sys_xjob);
module_exit(exit_sys_xjob);
MODULE_LICENSE("GPL");
LICENSE("GPL");
