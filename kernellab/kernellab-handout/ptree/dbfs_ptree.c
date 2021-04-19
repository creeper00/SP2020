#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#define MAX 100000

MODULE_LICENSE("GPL");

static struct dentry *dir, *inputdir, *ptreedir;
static struct task_struct *curr;
struct debugfs_blob_wrapper blob;
char *buf;

void printer(struct task_struct *task)
{
	if(task->pid != 1) printer(task->real_parent);
	blob.size = blob.size + snprintf(blob.data+blob.size, MAX-blob.size, "%s (%d)\n", task->comm, task->pid);
} 

static ssize_t write_pid_to_input(struct file *fp, 
                                const char __user *user_buffer, 
                                size_t length, 
                                loff_t *position)
{
        pid_t input_pid;
	blob.size =0;
        sscanf(user_buffer, "%u", &input_pid);
        curr = get_pid_task(find_get_pid(input_pid), PIDTYPE_PID);// Find task_struct using input_pid. Hint: pid_task

        printer(curr);// Tracing process tree from input_pid to init(1) process

        // Make Output Format string: process_command (process_id)

        return length;
}

static const struct file_operations dbfs_fops = {
        .write = write_pid_to_input,
};

static int __init dbfs_module_init(void)
{
        // Implement init module code
	int siz = MAX * sizeof(char);
	buf = kmalloc(siz, GFP_KERNEL);
	blob.data = (void*) buf;
//#if 0
        dir = debugfs_create_dir("ptree", NULL);
        
        if (!dir) {
                printk("Cannot create ptree dir\n");
                return -1;
        }

        inputdir = debugfs_create_file("input",S_IWUSR,dir,NULL,&dbfs_fops);
        ptreedir = debugfs_create_blob("ptree",S_IRUSR,dir,&blob); // Find suitable debugfs API
//#endif
	
	//printk("dbfs_ptree module initialize done\n");

        return 0;
}

static void __exit dbfs_module_exit(void)
{
        // Implement exit module code
	kfree(buf);
	
	//printk("dbfs_ptree module exit\n");
}

module_init(dbfs_module_init);
module_exit(dbfs_module_exit);
