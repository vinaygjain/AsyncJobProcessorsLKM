#include "sys_xjob.h"

void hello_nl_recv_msg(struct sk_buff *skb)
{
	struct nlmsghdr *nlh;
	int pid;
	struct sk_buff *skb_out;
	int msg_size;
	char *msg, *user_cmd, temp[4];
	int res;
	long job_id;
	int err;

	nlh = (struct nlmsghdr *)skb->data;
	user_cmd = (char *)nlmsg_data(nlh);
	printk(KERN_INFO "Netlink received msg payload:%s\n",
		user_cmd);
	pid = nlh->nlmsg_pid; /*pid of sending process */
	strncpy(temp, user_cmd, 4);

	/* check if msg is read checksum or list */
	if (strcmp(user_cmd, "res:0") == 0)
		msg = enumerate_results();
	else if  (strcmp(user_cmd, "list:0") == 0)
		msg = enumerate_jobs();
	else {
		UDBG;
		err = kstrtol(user_cmd, 10, &job_id);
		if (!err)
			msg = enumerate_one_result(job_id);
		else {
			printk(KERN_CRIT "Error in kstrtol.\n");
			return;
		}
	}

	msg_size = strlen(msg);
	skb_out = nlmsg_new(msg_size, 0);

	if (!skb_out) {
		printk(KERN_ERR "Failed to allocate new skb\n");
		return;
	}

	nlh = nlmsg_put(skb_out, 0, 0, NLMSG_DONE, msg_size, 0);
	NETLINK_CB(skb_out).dst_group = 0; /* not in mcast group */
	strncpy(nlmsg_data(nlh), msg, msg_size);
	printk(KERN_DEBUG "Sending msg = %s\n", msg);
	res = nlmsg_unicast(nl_sk, skb_out, pid);

	if (res < 0)
		printk(KERN_INFO "Error while sending bak to user\n");
}

int __send_signal(int msg, int pid)
{
	int err;
	struct siginfo info;
	struct task_struct *t;

	/* real time signals may have 32 bits of data. */
	memset(&info, 0, sizeof(struct siginfo));
	info.si_signo = SIG_TEST;
	info.si_code = SI_QUEUE;
	info.si_int = msg;

	rcu_read_lock();
		t = pid_task(find_pid_ns(pid, &init_pid_ns), PIDTYPE_PID);
		if (t == NULL) {
			printk(KERN_CRIT "No such pid.\n");
			rcu_read_unlock();
			return -ENODEV;
		}
	rcu_read_unlock();

	/* send the signal */
	err = send_sig_info(SIG_TEST, &info, t);

	return err;
}

