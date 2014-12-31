#include <linux/linkage.h>
#include <linux/moduleloader.h>
#include <linux/compiler.h>
#include "common.h"

/* For file operations. */
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/fsnotify.h>
#include <linux/uaccess.h>
#include <linux/unistd.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/fcntl.h>

/* For threading. */
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/delay.h>

/* For locking. */
#include <linux/spinlock.h>
#include <linux/wait.h>

/* For Networking. */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <net/sock.h>
#include <linux/socket.h>
#include <linux/net.h>
#include <linux/types.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>

/* Crypto API. */
#include <linux/crc32c.h>
#include <linux/crypto.h>

/* For Signalling. */
#include <asm/siginfo.h>	/* for siginfo */
#include <linux/rcupdate.h>	/* rcu_read_lock */
#include <linux/sched.h>	/* find_task_by_pid_type */
#include <linux/pid.h>		/* for pid */

/* Job states. */
#define S_RUNNING	1
#define S_WAITING	2
#define S_COMPLETE	3

/* Operations. */
#define ADMIN	1
#define USER	2
#define OP_LIST	1
#define OP_REMOVE	2
#define OP_CHECKSUM	1
#define OP_ENCRYPT	2
#define OP_COMPRESS	3

/* Queue. */
#define QMAX	1000
#define RETRY_MAX	5

/* Netlink. */
#define NETLINK_USER	31
#define MAX_PAYLOAD 1024 /* maximum payload size*/

/* Signal. */
#define SIG_TEST	44	/* we choose 44 as our signal number */

/* Debugging. */
#define UDBG	printk(KERN_DEBUG "UDBG %s - %d\n", __func__, __LINE__)

extern int produce(struct job *j);
extern int do_encrypt(struct job *job, int id);
extern int encryption_operation(struct job *ja, struct job *j);
extern int do_checksum(struct job *job, int id);
extern int checksum_operation(struct job *ja, struct job *j);
extern void add_result(int qid, int pid, char *res, int len);
extern int __send_signal(int msg, int pid);
extern void hello_nl_recv_msg(struct sk_buff *skb);
extern char *enumerate_jobs(void);
extern char *enumerate_results(void);
extern char *enumerate_one_result(int id);
extern struct sock *nl_sk;		/* Socket for netlink */
extern void send_msg_pid(int pid);
extern int __send_signal(int msg, int pid);
