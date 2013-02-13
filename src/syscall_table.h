#include <linux/futex.h>
#include <sys/syscall.h>

/* #if defined(__x86_64__) */
/* #  include "vki-scnums-amd64-linux.h" */
/* #elif defined(__i386__) */
/* #  include "vki-scnums-x86-linux.h" */
/* #elif defined(__ARMEL__) */
/* #  include "vki-scnums-arm-linux.h" */
/* #else */
/* #  error "Unknown architecture!" */
/* #endif */

static char *syscall_table[] = {
	[SYS_restart_syscall] = "restart_syscall",
	[SYS_exit] = "exit",
	[SYS_fork] = "fork",
	[SYS_read] = "read",
	[SYS_write] = "write",
	[SYS_open] = "open",
	[SYS_close] = "close",
#ifdef SYS_waitpid
	[SYS_waitpid] = "waitpid",
#endif
	[SYS_creat] = "creat",
	[SYS_link] = "link",
	[SYS_unlink] = "unlink",
	[SYS_execve] = "execve",
	[SYS_chdir] = "chdir",
#ifdef SYS_time
	[SYS_time] = "time",
#endif
	[SYS_mknod] = "mknod",
	[SYS_chmod] = "chmod",
	[SYS_lchown] = "lchown",
#ifdef SYS_break
	[SYS_break] = "break",
#endif
#ifdef SYS_oldstat
	[SYS_oldstat] = "oldstat",
#endif
	[SYS_lseek] = "lseek",
	[SYS_getpid] = "getpid",
#ifdef SYS_mount
	[SYS_mount] = "mount",
#endif
#ifdef SYS_umount
	[SYS_umount] = "umount",
#endif
	[SYS_setuid] = "setuid",
	[SYS_getuid] = "getuid",
#ifdef SYS_stime
	[SYS_stime] = "stime",
#endif
	[SYS_ptrace] = "ptrace",
#ifdef SYS_alarm
	[SYS_alarm] = "alarm",
#endif
#ifdef SYS_oldfstat
	[SYS_oldfstat] = "oldfstat",
#endif
	[SYS_pause] = "pause",
#ifdef SYS_utime
	[SYS_utime] = "utime",
#endif
#ifdef SYS_stty
	[SYS_stty] = "stty",
#endif
#ifdef SYS_gtty
	[SYS_gtty] = "gtty",
#endif
	[SYS_access] = "access",
	[SYS_nice] = "nice",
#ifdef SYS_ftime
	[SYS_ftime] = "ftime",
#endif
	[SYS_sync] = "sync",
	[SYS_kill] = "kill",
	[SYS_rename] = "rename",
	[SYS_mkdir] = "mkdir",
	[SYS_rmdir] = "rmdir",
	[SYS_dup] = "dup",
	[SYS_pipe] = "pipe",
	[SYS_times] = "times",
#ifdef SYS_prof
	[SYS_prof] = "prof",
#endif
	[SYS_brk] = "brk",
	[SYS_setgid] = "setgid",
	[SYS_getgid] = "getgid",
#ifdef SYS_signal
	[SYS_signal] = "signal",
#endif
	[SYS_geteuid] = "geteuid",
	[SYS_getegid] = "getegid",
	[SYS_acct] = "acct",
	[SYS_umount2] = "umount2",
#ifdef SYS_lock
	[SYS_lock] = "lock",
#endif
	[SYS_ioctl] = "ioctl",
	[SYS_fcntl] = "fcntl",
#ifdef SYS_mpx
	[SYS_mpx] = "mpx",
#endif
	[SYS_setpgid] = "setpgid",
#ifdef SYS_ulimit
	[SYS_ulimit] = "ulimit",
#endif
#ifdef SYS_oldolduname
	[SYS_oldolduname] = "oldolduname",
#endif
	[SYS_umask] = "umask",
	[SYS_chroot] = "chroot",
	[SYS_ustat] = "ustat",
	[SYS_dup2] = "dup2",
	[SYS_getppid] = "getppid",
	[SYS_getpgrp] = "getpgrp",
	[SYS_setsid] = "setsid",
	[SYS_sigaction] = "sigaction",
#ifdef SYS_sgetmask
	[SYS_sgetmask] = "sgetmask",
#endif
#ifdef SYS_ssetmask
	[SYS_ssetmask] = "ssetmask",
#endif
	[SYS_setreuid] = "setreuid",
	[SYS_setregid] = "setregid",
	[SYS_sigsuspend] = "sigsuspend",
	[SYS_sigpending] = "sigpending",
	[SYS_sethostname] = "sethostname",
	[SYS_setrlimit] = "setrlimit",
#ifdef SYS_getrlimit
	[SYS_getrlimit] = "getrlimit",
#endif
	[SYS_getrusage] = "getrusage",
	[SYS_gettimeofday] = "gettimeofday",
	[SYS_settimeofday] = "settimeofday",
	[SYS_getgroups] = "getgroups",
	[SYS_setgroups] = "setgroups",
#ifdef SYS_select
	[SYS_select] = "_oldselect",
#endif
	[SYS_symlink] = "symlink",
#ifdef SYS_oldlstat
	[SYS_oldlstat] = "oldlstat",
#endif
	[SYS_readlink] = "readlink",
	[SYS_uselib] = "uselib",
	[SYS_swapon] = "swapon",
	[SYS_reboot] = "reboot",
#ifdef SYS_readdir
	[SYS_readdir] = "readdir",
#endif
#ifdef SYS_mmap
	[SYS_mmap] = "mmap",
#endif
	[SYS_munmap] = "munmap",
	[SYS_truncate] = "truncate",
	[SYS_ftruncate] = "ftruncate",
	[SYS_fchmod] = "fchmod",
	[SYS_fchown] = "fchown",
	[SYS_getpriority] = "getpriority",
	[SYS_setpriority] = "setpriority",
#ifdef SYS_profil
	[SYS_profil] = "profil",
#endif
	[SYS_statfs] = "statfs",
#ifdef SYS_fstatfs
	[SYS_fstatfs] = "fstatfs",
#endif
#ifdef SYS_ioperm
	[SYS_ioperm] = "ioperm",
#endif
#ifdef SYS_socketcall
	[SYS_socketcall] = "socketcall",
#endif
	[SYS_syslog] = "syslog",
	[SYS_setitimer] = "setitimer",
	[SYS_getitimer] = "getitimer",
	[SYS_stat] = "stat",
	[SYS_lstat] = "lstat",
	[SYS_fstat] = "fstat",
#ifdef SYS_olduname
	[SYS_olduname] = "olduname",
#endif
#ifdef SYS_iopl
	[SYS_iopl] = "iopl",
#endif
	[SYS_vhangup] = "vhangup",
#ifdef SYS_idle
	[SYS_idle] = "idle",
#endif
#ifdef SYS_vm86old
	[SYS_vm86old] = "vm86old",
#endif
	[SYS_wait4] = "wait4",
	[SYS_swapoff] = "swapoff",
	[SYS_sysinfo] = "sysinfo",
#ifdef SYS_ipc
	[SYS_ipc] = "ipc",
#endif
	[SYS_fsync] = "fsync",
	[SYS_sigreturn] = "sigreturn",
	[SYS_clone] = "clone",
	[SYS_setdomainname] = "setdomainname",
	[SYS_uname] = "uname",
#ifdef SYS_modify_ldt
	[SYS_modify_ldt] = "modify_ldt",
#endif
	[SYS_adjtimex] = "adjtimex",
	[SYS_mprotect] = "mprotect",
	[SYS_sigprocmask] = "sigprocmask",
#ifdef SYS_create_module
	[SYS_create_module] = "create_module",
#endif
	[SYS_init_module] = "init_module",
	[SYS_delete_module] = "delete_module",
#ifdef SYS_get_kernel_syms
	[SYS_get_kernel_syms] = "get_kernel_syms",
#endif
	[SYS_quotactl] = "quotactl",
	[SYS_getpgid] = "getpgid",
	[SYS_fchdir] = "fchdir",
	[SYS_bdflush] = "bdflush",
	[SYS_sysfs] = "sysfs",
	[SYS_personality] = "personality",
#ifdef SYS_afs_syscall
	[SYS_afs_syscall] = "afs_syscall",
#endif
	[SYS_setfsuid] = "setfsuid",
	[SYS_setfsgid] = "setfsgid",
	[SYS__llseek] = "_llseek",
	[SYS_getdents] = "getdents",
	[SYS__newselect] = "select",
	[SYS_flock] = "flock",
	[SYS_msync] = "msync",
	[SYS_readv] = "readv",
	[SYS_writev] = "writev",
	[SYS_getsid] = "getsid",
	[SYS_fdatasync] = "fdatasync",
	[SYS__sysctl] = "_sysctl",
	[SYS_mlock] = "mlock",
	[SYS_munlock] = "munlock",
	[SYS_mlockall] = "mlockall",
	[SYS_munlockall] = "munlockall",
	[SYS_sched_setparam] = "sched_setparam",
	[SYS_sched_getparam] = "sched_getparam",
	[SYS_sched_setscheduler] = "sched_setscheduler",
	[SYS_sched_getscheduler] = "sched_getscheduler",
	[SYS_sched_yield] = "sched_yield",
	[SYS_sched_get_priority_max] = "sched_get_priority_max",
	[SYS_sched_get_priority_min] = "sched_get_priority_min",
	[SYS_sched_rr_get_interval] = "sched_rr_get_interval",
	[SYS_nanosleep] = "nanosleep",
	[SYS_mremap] = "mremap",
	[SYS_setresuid] = "setresuid",
	[SYS_getresuid] = "getresuid",
#ifdef SYS_vm86
	[SYS_vm86] = "vm86",
#endif
#ifdef SYS_query_module
	[SYS_query_module] = "query_module",
#endif
	[SYS_poll] = "poll",
	[SYS_nfsservctl] = "nfsservctl",
	[SYS_setresgid] = "setresgid",
	[SYS_getresgid] = "getresgid",
	[SYS_prctl] = "prctl",
	[SYS_rt_sigreturn] = "rt_sigreturn",
	[SYS_rt_sigaction] = "rt_sigaction",
	[SYS_rt_sigprocmask] = "rt_sigprocmask",
	[SYS_rt_sigpending] = "rt_sigpending",
	[SYS_rt_sigtimedwait] = "rt_sigtimedwait",
	[SYS_rt_sigqueueinfo] = "rt_sigqueueinfo",
	[SYS_rt_sigsuspend] = "rt_sigsuspend",
	[SYS_pread64] = "pread64",
	[SYS_pwrite64] = "pwrite64",
	[SYS_chown] = "chown",
	[SYS_getcwd] = "getcwd",
	[SYS_capget] = "capget",
	[SYS_capset] = "capset",
	[SYS_sigaltstack] = "sigaltstack",
	[SYS_sendfile] = "sendfile",
#ifdef SYS_getpmsg
	[SYS_getpmsg] = "getpmsg",
#endif
#ifdef SYS_putpmsg
	[SYS_putpmsg] = "putpmsg",
#endif
	[SYS_vfork] = "vfork",
	[SYS_ugetrlimit] = "ugetrlimit",
	[SYS_mmap2] = "mmap2",
	[SYS_truncate64] = "truncate64",
	[SYS_ftruncate64] = "ftruncate64",
	[SYS_stat64] = "stat64",
	[SYS_lstat64] = "lstat64",
	[SYS_fstat64] = "fstat64",
	[SYS_lchown32] = "lchown32",
	[SYS_getuid32] = "getuid32",
	[SYS_getgid32] = "getgid32",
	[SYS_geteuid32] = "geteuid32",
	[SYS_getegid32] = "getegid32",
	[SYS_setreuid32] = "setreuid32",
	[SYS_setregid32] = "setregid32",
	[SYS_getgroups32] = "getgroups32",
	[SYS_setgroups32] = "setgroups32",
	[SYS_fchown32] = "fchown32",
	[SYS_setresuid32] = "setresuid32",
	[SYS_getresuid32] = "getresuid32",
	[SYS_setresgid32] = "setresgid32",
	[SYS_getresgid32] = "getresgid32",
	[SYS_chown32] = "chown32",
	[SYS_setuid32] = "setuid32",
	[SYS_setgid32] = "setgid32",
	[SYS_setfsuid32] = "setfsuid32",
	[SYS_setfsgid32] = "setfsgid32",
	[SYS_pivot_root] = "pivot_root",
	[SYS_mincore] = "mincore",
	[SYS_madvise] = "madvise",
	[SYS_getdents64] = "getdents64",
	[SYS_fcntl64] = "fcntl64",
	[SYS_gettid] = "gettid",
	[SYS_readahead] = "readahead",
	[SYS_setxattr] = "setxattr",
	[SYS_lsetxattr] = "lsetxattr",
	[SYS_fsetxattr] = "fsetxattr",
	[SYS_getxattr] = "getxattr",
	[SYS_lgetxattr] = "lgetxattr",
	[SYS_fgetxattr] = "fgetxattr",
	[SYS_listxattr] = "listxattr",
	[SYS_llistxattr] = "llistxattr",
	[SYS_flistxattr] = "flistxattr",
	[SYS_removexattr] = "removexattr",
	[SYS_lremovexattr] = "lremovexattr",
	[SYS_fremovexattr] = "fremovexattr",
	[SYS_tkill] = "tkill",
	[SYS_sendfile64] = "sendfile64",
	[SYS_futex] = "futex",
	[SYS_sched_setaffinity] = "sched_setaffinity",
	[SYS_sched_getaffinity] = "sched_getaffinity",
#ifdef SYS_set_thread_area
	[SYS_set_thread_area] = "set_thread_area",
#endif
#ifdef SYS_get_thread_area
	[SYS_get_thread_area] = "get_thread_area",
#endif
	[SYS_io_setup] = "io_setup",
	[SYS_io_destroy] = "io_destroy",
	[SYS_io_getevents] = "io_getevents",
	[SYS_io_submit] = "io_submit",
	[SYS_io_cancel] = "io_cancel",
#ifdef SYS_fadvise64
	[SYS_fadvise64] = "fadvise64",
#endif
	[SYS_exit_group] = "exit_group",
	[SYS_lookup_dcookie] = "lookup_dcookie",
	[SYS_epoll_create] = "epoll_create",
	[SYS_epoll_ctl] = "epoll_ctl",
	[SYS_epoll_wait] = "epoll_wait",
	[SYS_remap_file_pages] = "remap_file_pages",
	[SYS_set_tid_address] = "set_tid_address",
	[SYS_timer_create] = "timer_create",
	[SYS_timer_settime] = "timer_settime",
	[SYS_timer_gettime] = "timer_gettime",
	[SYS_timer_getoverrun] = "timer_getoverrun",
	[SYS_timer_delete] = "timer_delete",
	[SYS_clock_settime] = "clock_settime",
	[SYS_clock_gettime] = "clock_gettime",
	[SYS_clock_getres] = "clock_getres",
	[SYS_clock_nanosleep] = "clock_nanosleep",
	[SYS_statfs64] = "statfs64",
	[SYS_fstatfs64] = "fstatfs64",
	[SYS_tgkill] = "tgkill",
	[SYS_utimes] = "utimes",
#ifdef SYS_fadvise64_64
	[SYS_fadvise64_64] = "fadvise64_64",
#endif
	[SYS_vserver] = "vserver",
	[SYS_mbind] = "mbind",
	[SYS_get_mempolicy] = "get_mempolicy",
	[SYS_set_mempolicy] = "set_mempolicy",
	[SYS_mq_open] = "mq_open",
	[SYS_mq_unlink] = "mq_unlink",
	[SYS_mq_timedsend] = "mq_timedsend",
	[SYS_mq_timedreceive] = "mq_timedreceive",
	[SYS_mq_notify] = "mq_notify",
	[SYS_mq_getsetattr] = "mq_getsetattr",
	[SYS_waitid] = "waitid",
	[SYS_add_key] = "add_key",
	[SYS_request_key] = "request_key",
	[SYS_keyctl] = "keyctl",
	[SYS_ioprio_set] = "ioprio_set",
	[SYS_ioprio_get] = "ioprio_get",
	[SYS_inotify_init] = "inotify_init",
	[SYS_inotify_add_watch] = "inotify_add_watch",
	[SYS_inotify_rm_watch] = "inotify_rm_watch",
#ifdef SYS_migrate_pages
	[SYS_migrate_pages] = "migrate_pages",
#endif
	[SYS_openat] = "openat",
	[SYS_mkdirat] = "mkdirat",
	[SYS_mknodat] = "mknodat",
	[SYS_fchownat] = "fchownat",
	[SYS_futimesat] = "futimesat",
	[SYS_fstatat64] = "fstatat64",
	[SYS_unlinkat] = "unlinkat",
	[SYS_renameat] = "renameat",
	[SYS_linkat] = "linkat",
	[SYS_symlinkat] = "symlinkat",
	[SYS_readlinkat] = "readlinkat",
	[SYS_fchmodat] = "fchmodat",
	[SYS_faccessat] = "faccessat",
	[SYS_pselect6] = "pselect6",
	[SYS_ppoll] = "ppoll",
	[SYS_unshare] = "unshare",
	[SYS_set_robust_list] = "set_robust_list",
	[SYS_get_robust_list] = "get_robust_list",
	[SYS_splice] = "splice",
#ifdef SYS_sync_file_range
	[SYS_sync_file_range] = "sync_file_range",
#endif
#ifdef SYS_arm_sync_file_range
	[SYS_arm_sync_file_range] = "sync_file_range",
#endif
	[SYS_tee] = "tee",
	[SYS_vmsplice] = "vmsplice",
	[SYS_move_pages] = "move_pages",
	[SYS_getcpu] = "getcpu",
	[SYS_epoll_pwait] = "epoll_pwait",
	[SYS_utimensat] = "utimensat",
	[SYS_signalfd] = "signalfd",
	[SYS_timerfd_create] = "timerfd_create",
	[SYS_eventfd] = "eventfd",
	[SYS_fallocate] = "fallocate",
	[SYS_timerfd_settime] = "timerfd_settime",
	[SYS_timerfd_gettime] = "timerfd_gettime",
	[SYS_signalfd4] = "signalfd4",
	[SYS_eventfd2] = "eventfd2",
	[SYS_epoll_create1] = "epoll_create1",
	[SYS_dup3] = "dup3",
	[SYS_pipe2] = "pipe2",
	[SYS_inotify_init1] = "inotify_init1",
	[SYS_preadv] = "preadv",
	[SYS_pwritev] = "pwritev",
	[SYS_rt_tgsigqueueinfo] = "rt_tgsigqueueinfo",
	[SYS_perf_event_open] = "perf_event_open",
	[SYS_recvmmsg] = "recvmmsg",
	[SYS_fanotify_init] = "fanotify_init",
	[SYS_fanotify_mark] = "fanotify_mark",
	[SYS_prlimit64] = "prlimit64",
	[SYS_name_to_handle_at] = "name_to_handle_at",
	[SYS_open_by_handle_at] = "open_by_handle_at",
	[SYS_clock_adjtime] = "clock_adjtime",
	[SYS_syncfs] = "syncfs",
	[SYS_sendmmsg] = "sendmmsg",
	[SYS_setns] = "setns",
	[SYS_process_vm_readv] = "process_vm_readv",
	[SYS_process_vm_writev] = "process_vm_writev"
};
