#include <linux/sched.h>
#include <linux/file.h>
#include <linux/fdtable.h>
#include <linux/atomic.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/kallsyms.h>
#include <linux/version.h>
#include <linux/ipc_namespace.h>
#include <linux/task_work.h>
#include <linux/kprobes.h>
#include <linux/cred.h>
#include <linux/errno.h>

/*
 * On kernel 5.7 and later, kallsyms_lookup_name() can no longer be called from a kernel
 * module for reasons described here: https://lwn.net/Articles/813350/
 * As binder really needs to use kallsysms_lookup_name() to access some kernel
 * functions that otherwise wouldn't be accessible, KProbes are used on later
 * kernels to get the address of kallsysms_lookup_name(). The function is
 * afterwards used just as before. This is a very dirty hack though and the much
 * better solution would be if all the functions that are currently resolved
 * with kallsysms_lookup_name() would get an EXPORT_SYMBOL() annotation to
 * make them directly accessible to kernel modules.
 */
typedef unsigned long (*kallsyms_lookup_name_t)(const char *name);

static int dummy_kprobe_handler(struct kprobe *p, struct pt_regs *regs)
{
	return 0;
}

static kallsyms_lookup_name_t get_kallsyms_lookup_name_ptr(void)
{
	struct kprobe probe;
	int ret;
	kallsyms_lookup_name_t addr;

	memset(&probe, 0, sizeof(probe));
	probe.pre_handler = dummy_kprobe_handler;
	probe.symbol_name = "kallsyms_lookup_name";
	ret = register_kprobe(&probe);
	if (ret)
		return NULL;
	addr = (kallsyms_lookup_name_t)probe.addr;
	unregister_kprobe(&probe);

	return addr;
}

static unsigned long kallsyms_lookup_name_wrapper(const char *name)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0))
	static kallsyms_lookup_name_t func_ptr = NULL;
	if (!func_ptr)
		func_ptr = get_kallsyms_lookup_name_ptr();

	if (!func_ptr)
	{
		pr_err_once("binder: kallsyms_lookup_name unavailable (kprobe failed)\n");
		return 0;
	}
	return func_ptr(name);
#else
	return kallsyms_lookup_name(name);
#endif
}

typedef void (*zap_page_range_ptr_t)(struct vm_area_struct *, unsigned long, unsigned long);
static zap_page_range_ptr_t zap_page_range_ptr = NULL;
void zap_page_range(struct vm_area_struct *vma, unsigned long address, unsigned long size)
{
	if (!zap_page_range_ptr)
		zap_page_range_ptr = (zap_page_range_ptr_t)kallsyms_lookup_name_wrapper("zap_page_range");
	if (WARN_ON_ONCE(!zap_page_range_ptr))
		return;
	zap_page_range_ptr(vma, address, size);
}

typedef int (*can_nice_ptr_t)(const struct task_struct *, const int);
static can_nice_ptr_t can_nice_ptr = NULL;
int can_nice(const struct task_struct *p, const int nice)
{
	if (!can_nice_ptr)
		can_nice_ptr = (can_nice_ptr_t)kallsyms_lookup_name_wrapper("can_nice");
	if (WARN_ON_ONCE(!can_nice_ptr))
		return -ENOSYS;
	return can_nice_ptr(p, nice);
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 2))
typedef int (*security_binder_set_context_mgr_ptr_t)(const struct cred *mgr);
static security_binder_set_context_mgr_ptr_t security_binder_set_context_mgr_ptr = NULL;
int security_binder_set_context_mgr(const struct cred *mgr)
#else
typedef int (*security_binder_set_context_mgr_ptr_t)(struct task_struct *mgr);
static security_binder_set_context_mgr_ptr_t security_binder_set_context_mgr_ptr = NULL;
int security_binder_set_context_mgr(struct task_struct *mgr)
#endif
{
	if (!security_binder_set_context_mgr_ptr)
		security_binder_set_context_mgr_ptr = (security_binder_set_context_mgr_ptr_t)kallsyms_lookup_name_wrapper("security_binder_set_context_mgr");
	if (WARN_ON_ONCE(!security_binder_set_context_mgr_ptr))
		return -ENOSYS;
	return security_binder_set_context_mgr_ptr(mgr);
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 2))
typedef int (*security_binder_transaction_ptr_t)(const struct cred *from, const struct cred *to);
static security_binder_transaction_ptr_t security_binder_transaction_ptr = NULL;
int security_binder_transaction(const struct cred *from, const struct cred *to)
#else
typedef int (*security_binder_transaction_ptr_t)(struct task_struct *from, struct task_struct *to);
static security_binder_transaction_ptr_t security_binder_transaction_ptr = NULL;
int security_binder_transaction(struct task_struct *from, struct task_struct *to)
#endif
{
	if (!security_binder_transaction_ptr)
		security_binder_transaction_ptr = (security_binder_transaction_ptr_t)kallsyms_lookup_name_wrapper("security_binder_transaction");
	if (WARN_ON_ONCE(!security_binder_transaction_ptr))
		return -ENOSYS;
	return security_binder_transaction_ptr(from, to);
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 2))
typedef int (*security_binder_transfer_binder_ptr_t)(const struct cred *from, const struct cred *to);
static security_binder_transfer_binder_ptr_t security_binder_transfer_binder_ptr = NULL;
int security_binder_transfer_binder(const struct cred *from, const struct cred *to)
#else
typedef int (*security_binder_transfer_binder_ptr_t)(struct task_struct *from, struct task_struct *to);
static security_binder_transfer_binder_ptr_t security_binder_transfer_binder_ptr = NULL;
int security_binder_transfer_binder(struct task_struct *from, struct task_struct *to)
#endif
{
	if (!security_binder_transfer_binder_ptr)
		security_binder_transfer_binder_ptr = (security_binder_transfer_binder_ptr_t)kallsyms_lookup_name_wrapper("security_binder_transfer_binder");
	if (WARN_ON_ONCE(!security_binder_transfer_binder_ptr))
		return -ENOSYS;
	return security_binder_transfer_binder_ptr(from, to);
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 2))
typedef int (*security_binder_transfer_file_ptr_t)(const struct cred *from, const struct cred *to, const struct file *file);
static security_binder_transfer_file_ptr_t security_binder_transfer_file_ptr = NULL;
int security_binder_transfer_file(const struct cred *from, const struct cred *to, const struct file *file)
#else
typedef int (*security_binder_transfer_file_ptr_t)(struct task_struct *from, struct task_struct *to, struct file *file);
static security_binder_transfer_file_ptr_t security_binder_transfer_file_ptr = NULL;
int security_binder_transfer_file(struct task_struct *from, struct task_struct *to, struct file *file)
#endif
{
	if (!security_binder_transfer_file_ptr)
		security_binder_transfer_file_ptr = (security_binder_transfer_file_ptr_t)kallsyms_lookup_name_wrapper("security_binder_transfer_file");
	if (WARN_ON_ONCE(!security_binder_transfer_file_ptr))
		return -ENOSYS;
	return security_binder_transfer_file_ptr(from, to, file);
}

typedef void (*put_ipc_ns_ptr_t)(struct ipc_namespace *ns);
static put_ipc_ns_ptr_t put_ipc_ns_ptr = NULL;
void put_ipc_ns(struct ipc_namespace *ns)
{
	if (!put_ipc_ns_ptr)
		put_ipc_ns_ptr = (put_ipc_ns_ptr_t)kallsyms_lookup_name_wrapper("put_ipc_ns");
	if (WARN_ON_ONCE(!put_ipc_ns_ptr))
		return;
	put_ipc_ns_ptr(ns);
}

// struct ipc_namespace init_ipc_ns;
typedef struct ipc_namespace *init_ipc_ns_ptr_t;
static init_ipc_ns_ptr_t init_ipc_ns_ptr = NULL;
init_ipc_ns_ptr_t get_init_ipc_ns_ptr(void)
{
	if (!init_ipc_ns_ptr)
		init_ipc_ns_ptr = (init_ipc_ns_ptr_t)kallsyms_lookup_name_wrapper("init_ipc_ns");
	return init_ipc_ns_ptr;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 8, 0)
typedef int (*task_work_add_ptr_t)(struct task_struct *task, struct callback_head *twork, enum task_work_notify_mode notify);
static task_work_add_ptr_t task_work_add_ptr = NULL;
int task_work_add(struct task_struct *task, struct callback_head *twork, enum task_work_notify_mode notify)
#else
typedef int (*task_work_add_ptr_t)(struct task_struct *task, struct callback_head *twork, bool notify);
static task_work_add_ptr_t task_work_add_ptr = NULL;
int task_work_add(struct task_struct *task, struct callback_head *twork, bool notify)
#endif
{
	if (!task_work_add_ptr)
		task_work_add_ptr = (task_work_add_ptr_t)kallsyms_lookup_name_wrapper("task_work_add");
	if (WARN_ON_ONCE(!task_work_add_ptr))
		return -ENOSYS;
	return task_work_add_ptr(task, twork, notify);
}

typedef void (*mmput_async_ptr_t)(struct mm_struct *);
static mmput_async_ptr_t mmput_async_ptr = NULL;
void mmput_async(struct mm_struct *mm)
{
	if (!mmput_async_ptr)
		mmput_async_ptr = (mmput_async_ptr_t)kallsyms_lookup_name_wrapper("mmput_async");
	if (WARN_ON_ONCE(!mmput_async_ptr))
		return;
	mmput_async_ptr(mm);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 20, 1)
static inline void __clear_open_fd(unsigned int fd, struct fdtable *fdt)
{
	__clear_bit(fd, fdt->open_fds);
	__clear_bit(fd / BITS_PER_LONG, fdt->full_fds_bits);
}

static void __put_unused_fd(struct files_struct *files, unsigned int fd)
{
	struct fdtable *fdt = files_fdtable(files);
	__clear_open_fd(fd, fdt);
	if (fd < files->next_fd)
		files->next_fd = fd;
}

int __close_fd_get_file(unsigned int fd, struct file **res)
{
	struct files_struct *files = current->files;
	struct file *file;
	struct fdtable *fdt;

	spin_lock(&files->file_lock);
	fdt = files_fdtable(files);
	if (fd >= fdt->max_fds)
		goto out_unlock;
	file = fdt->fd[fd];
	if (!file)
		goto out_unlock;
	rcu_assign_pointer(fdt->fd[fd], NULL);
	__put_unused_fd(files, fd);
	spin_unlock(&files->file_lock);
	get_file(file);
	*res = file;
	return filp_close(file, files);

out_unlock:
	spin_unlock(&files->file_lock);
	*res = NULL;
	return -ENOENT;
}
#else
typedef int (*__close_fd_get_file_ptr_t)(unsigned int fd, struct file **res);
static __close_fd_get_file_ptr_t __close_fd_get_file_ptr = NULL;
int __close_fd_get_file(unsigned int fd, struct file **res)
{
	if (!__close_fd_get_file_ptr)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 8, 0)
		__close_fd_get_file_ptr = (__close_fd_get_file_ptr_t)kallsyms_lookup_name_wrapper("file_close_fd");
#else
		__close_fd_get_file_ptr = (__close_fd_get_file_ptr_t)kallsyms_lookup_name_wrapper("__close_fd_get_file");
#endif

	if (WARN_ON_ONCE(!__close_fd_get_file_ptr))
	{
		*res = NULL;
		return -ENOSYS;
	}
	return __close_fd_get_file_ptr(fd, res);
}
#endif // LINUX_VERSION_CODE < KERNEL_VERSION(4, 20, 1)

typedef struct file *(*close_fd_get_file_t)(unsigned int fd);
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 8, 0)
static close_fd_get_file_t close_fd_get_file_ptr = NULL;
struct file *file_close_fd(unsigned int fd)
{
	if (!close_fd_get_file_ptr)
		close_fd_get_file_ptr = (close_fd_get_file_t)kallsyms_lookup_name_wrapper("close_fd_get_file");
	if (!close_fd_get_file_ptr)
	{
		WARN_ON_ONCE(1);
		return NULL;
	}
	return close_fd_get_file_ptr(fd);
}
#endif
