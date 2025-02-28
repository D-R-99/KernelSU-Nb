#include <linux/module.h>
#include <linux/tty.h>
#include <linux/miscdevice.h>
#include "comm.h"
#include "memory.h"
#include "process.h"

#include <linux/kernel.h> 
#include <linux/module.h> 
#include <linux/proc_fs.h> 
#include <linux/sched.h> 
#include <linux/uaccess.h> 
#include <linux/version.h> 
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0) 
#include <linux/minmax.h> 
#endif 
#include <linux/init.h>
#include <linux/kobject.h>
#include <linux/list.h>
#include <linux/slab.h>     // Memory allocation (kfree)
#include <linux/sysfs.h>    // Sysfs management

 
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0) 
#define HAVE_PROC_OPS 
#endif 
 
#define PROCFS_MAX_SIZE 2048UL 
#define PROCFS_ENTRY_FILENAME "exianb" 
#define DEVICE_NAME "exianb"
static char *my_string = "exianb";
// static struct proc_dir_entry *our_proc_file; 

static struct miscdevice dispatch_misc_device;
module_param(my_string, charp, 0644); // String parameter
MODULE_PARM_DESC(my_string, "Parameter");

int dispatch_open(struct inode *node, struct file *file) {
    return 0;
}

int dispatch_close(struct inode *node, struct file *file) {
    return 0;
}

long dispatch_ioctl(struct file* const file, unsigned int const cmd, unsigned long const arg) {
    static COPY_MEMORY cm;
    static MODULE_BASE mb;
    static char name[0x100] = {0};

    switch (cmd) {
        case OP_READ_MEM:
            {
                if (copy_from_user(&cm, (void __user*)arg, sizeof(cm)) != 0) {
                    pr_err("OP_READ_MEM copy_from_user failed.\n");
                    return -1;
                }
                if (read_process_memory(cm.pid, cm.addr, cm.buffer, cm.size, false) == false) {
                    pr_err("OP_READ_MEM read_process_memory failed.\n");
                    return -1;
                }
            }
            break;
	case OP_RW_MEM:
            {
                if (copy_from_user(&cm, (void __user*)arg, sizeof(cm)) != 0) {
                    pr_err("OP_READ_MEM copy_from_user failed.\n");
                    return -1;
                }
                if (read_process_memory(cm.pid, cm.addr, cm.buffer, cm.size, true) == false) {
                    pr_err("OP_READ_MEM read_process_memory failed.\n");
                    return -1;
                }
            }
            break;
        case OP_WRITE_MEM:
            {
                if (copy_from_user(&cm, (void __user*)arg, sizeof(cm)) != 0) {
                    return -1;
                }
                if (write_process_memory(cm.pid, cm.addr, cm.buffer, cm.size) == false) {
                    return -1;
                }
            }
            break;
        case OP_MODULE_BASE:
            {
                if (copy_from_user(&mb, (void __user*)arg, sizeof(mb)) != 0 
                ||  copy_from_user(name, (void __user*)mb.name, sizeof(name)-1) !=0) {
                    pr_err("OP_MODULE_BASE copy_from_user failed.\n");
                    return -1;
                }
                mb.base = get_module_base(mb.pid, name);
                if (copy_to_user((void __user*)arg, &mb, sizeof(mb)) !=0) {
                    pr_err("OP_MODULE_BASE copy_to_user failed.\n");
                    return -1;
                }
            }
            break;
        default:
            break;
    }
return 0;
}

struct file_operations dispatch_functions = {
    .owner   = THIS_MODULE,
    .open    = dispatch_open,
    .release = dispatch_close,
    .unlocked_ioctl = dispatch_ioctl,
};

struct miscdevice misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = DEVICE_NAME,
	.fops = &dispatch_functions,
};
/*
#include <linux/kprobes.h>
#include <linux/ptrace.h>

static struct kprobe kp;

// Structure for user data
struct ioctl_cf {
    int fd;
    char name[15];
};

// Pre-handler for the kprobe
static int handler_pre(struct kprobe *p, struct pt_regs *regs)
{
    void __user *argp;
    struct ioctl_cf cf;
    unsigned long request;
    // unsigned long arg;
    int new_fd;

    // Check if the syscall is ioctl (syscall number 29)
    if (regs->regs[8] == 29) {
        request = regs->regs[1];  // x1 contains the ioctl request code
        argp = (void __user *)regs->regs[2]; // x2 contains user-space argument pointer

        // Check if the request is 0x666
        if (request == 0x666) {
            unsigned long fd_address = regs->regs[0]; // x0 contains file descriptor structure pointer
            
            // Verify that the request memory location is accessible
            if (fd_address && !copy_from_user(&cf, (void __user *)(fd_address + 16), sizeof(cf))) {
                pr_info("Intercepted ioctl(0x666) - Creating anonymous inode\n");

                // Create an anonymous inode 
                new_fd = anon_inode_getfd(cf.name, &dispatch_functions, 0, 2);
                if (new_fd >= 0) {
                    cf.fd = new_fd;

                    // Write back to user-space 
                    if (copy_to_user((void __user *)(fd_address + 16), &cf, sizeof(cf))) {
                        pr_err("Failed to copy data back to user-space\n");
                    } else {
                        pr_info("Anon inode created with fd: %d\n", new_fd);
                    }
                } else {
                    pr_err("Failed to create anonymous inode\n");
                }
            }
        }
    }
    return 0;
}
*/
static struct list_head *module_prev;         // Store previous module position
static struct kobject *kobject_prev;          // Store previous kobject
static struct kobject *kobject_parent_prev;   // Store parent kobject
static struct module_sect_attrs *sect_attrs_bkp;
static struct module_notes_attrs *notes_attrs_bkp;
static int module_hidden = 0;                 // Flag for module state

void module_hide(void) {
    if (module_hidden) // If already hidden, return
        return;

    // Store the module’s original list position and kobject references
    module_prev = THIS_MODULE->list.prev;
    kobject_prev = &THIS_MODULE->mkobj.kobj;
    kobject_parent_prev = THIS_MODULE->mkobj.kobj.parent;

    // Backup section and notes attributes
    sect_attrs_bkp = THIS_MODULE->sect_attrs;
    notes_attrs_bkp = THIS_MODULE->notes_attrs;

    // Remove from /proc/modules
    list_del(&THIS_MODULE->list);

    // Remove from /sys/module
    kobject_del(&THIS_MODULE->mkobj.kobj);
    
    THIS_MODULE->list.prev = (list_head *)0xDEAD000000000122LL;
    THIS_MODULE->state = MODULE_STATE_UNFORMED; // Change state to prevent loading
	
    THIS_MODULE->sect_attrs = NULL;
    THIS_MODULE->notes_attrs = NULL;

    module_hidden = (unsigned int)0x1;; // Mark module as hidden
}


int __init driver_entry(void) {
    int ret;
    pr_info("[+] device loaded");	
    
    dispatch_misc_device.minor = MISC_DYNAMIC_MINOR;
    dispatch_misc_device.name = my_string; // "exianb";
    dispatch_misc_device.fops = &dispatch_functions;
    
    ret = misc_register(&dispatch_misc_device);
    /*
    kp.symbol_name = "el0_svc_common";
    kp.pre_handler = handler_pre;

    ret = register_kprobe(&kp);
    if (ret < 0) {
        pr_err("Failed to register kprobe: %d\n", ret);
        return ret;
    }
    */

    module_hide();
    
    return ret;
}

void __exit driver_unload(void) {
    pr_info("[+] device unloaded");    
    misc_deregister(&dispatch_misc_device);
    // unregister_kprobe(&kp);
}

module_init(driver_entry);
module_exit(driver_unload);

MODULE_AUTHOR("exianb");
MODULE_DESCRIPTION("exianb");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
#endif
