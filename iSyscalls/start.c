#include <linux/kernel.h>
#include <linux/syscalls.h>

bool is_logging = false;
EXPORT_SYMBOL(is_logging);

SYSCALL_DEFINE0(startlog)
{
	is_logging = true;
	return 0;
}
