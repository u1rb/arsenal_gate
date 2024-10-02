```c
#include <time.h>
#include <errno.h>
#include <stdio.h>

static clockid_t phc_id(void)
{
    /* fetch the dynamic clock id that belongs to /dev/ptp0 */
    int fd = open("/dev/ptp0", O_RDWR);
    if (fd < 0)  perror("ptp open");

    clockid_t clk = FD_TO_CLOCKID(fd);   /* kernel macro */
    return clk;
}

int main(void)
{
    struct timespec ts;
    clock_gettime(phc_id(), &ts);        /* works even if link is idle */
    printf("NIC time  = %lld.%09ld s\n",
            (long long)ts.tv_sec, ts.tv_nsec);
}


```
