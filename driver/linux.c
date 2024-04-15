#include "linux.h"
#include "defs.h"

#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

int recacc_open(recacc_device* dev, const char* uio_name) {
    dev->fd = open(uio_name, O_RDWR);
    if (dev->fd == -1) {
        printf("Failed to open %s: %s\n", uio_name, strerror(errno));
        return errno;
    }

    dev->mem = mmap(NULL, RECACC_MEM_MAP_SIZE, PROT_WRITE | PROT_READ, MAP_SHARED, dev->fd, 0);
    if (dev->mem == MAP_FAILED) {
        int err = errno;
        printf("Failed to map memory: %s\n", strerror(errno));
        recacc_close(dev);
        return err;
    }
    printf("Mapped region at %p\n", dev->mem);

    return 0;
}

int recacc_close(recacc_device* dev) {
    int ret = 0;

    if (dev->mem) {
        ret = munmap(dev->mem, RECACC_MEM_MAP_SIZE);
        if (ret)
            return ret;
        dev->mem = 0;
    }

    if (dev->fd) {
        ret = close(dev->fd);
        if (ret)
            return ret;
        dev->fd = 0;
    }

    return ret;
}
