#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <asm/pgtable.h>

MODULE_LICENSE("GPL");

static struct dentry *dir, *output;
static struct task_struct *task;
struct packet {
	pid_t pid;
	unsigned long vaddr;
	unsigned long paddr;
};

static ssize_t read_output(struct file *fp,
                        char __user *user_buffer,
                        size_t length,
                        loff_t *position)
{
        // Implement read file operation
	struct packet *pckt;
	pid_t app_pid;
	unsigned long app_vaddr;
	unsigned long app_paddr;
	struct mm_struct *app_mm;
	pgd_t *pgd;
	p4d_t *p4d;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	struct page* pg;
		
	pckt = (struct packet *) user_buffer;
	
	// get pid and vaddr of app
	app_pid = pckt->pid;
	app_vaddr = pckt->vaddr;
	
	//get task_struct of app from pid -> get mm_struct from task_struct
	task = get_pid_task(find_get_pid(app_pid), PIDTYPE_PID);
	app_mm = task->mm;

	//get entries
	pgd = pgd_offset(app_mm, app_vaddr);
	p4d = p4d_offset(pgd, app_vaddr);
	pud = pud_offset(p4d, app_vaddr);
	pmd = pmd_offset(pud, app_vaddr);
	pte = pte_offset_map(pmd, app_vaddr);
	
	//find page
	pg = pte_page(*pte);
	
	//find physical addr from page
	app_paddr = page_to_phys(pg);
	pckt->paddr = app_paddr;
	return length;
}

static const struct file_operations dbfs_fops = {
        // Mapping file operations with your functions
	.read = read_output,
};

static int __init dbfs_module_init(void)
{
        // Implement init module

//#if 0
        dir = debugfs_create_dir("paddr", NULL);

        if (!dir) {
                printk("Cannot create paddr dir\n");
                return -1;
        }

        // Fill in the arguments below
        output = debugfs_create_file("output",S_IWUSR,dir,NULL,&dbfs_fops);
//#endif

	//printk("dbfs_paddr module initialize done\n");

        return 0;
}

static void __exit dbfs_module_exit(void)
{
        // Implement exit module

	//printk("dbfs_paddr module exit\n");
}

module_init(dbfs_module_init);
module_exit(dbfs_module_exit);
