#ifndef COMPAT_SYS_WAIT_H
#define COMPAT_SYS_WAIT_H

static int wait(int *x)
{
	return -1;
};

static int waitpid(int x, int *y, int z)
{
	return -1;
}

#endif /* COMPAT_SYS_WAIT_H */
