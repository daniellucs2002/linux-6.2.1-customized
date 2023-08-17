#include <linux/kernel.h>
#include <linux/syscalls.h>

extern bool is_logging;

SYSCALL_DEFINE0(stoplog)
{
	is_logging = false;
	return 0;
}
