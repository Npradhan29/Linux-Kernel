#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <string.h>

#define DRIVER_NAME "/dev/vicharak"
#define PUSH_DATA _IOW('a', 'b', struct data)

struct data {
    int length;
    char *data;
};

int main(void) {
    int fd = open(DRIVER_NAME, O_RDWR);
    if (fd < 0) {
        perror("Failed to open device");
        return -1;
    }

    struct data *d = malloc(sizeof(struct data));
    d->length = 3;
    d->data = malloc(3);
    memcpy(d->data, "xyz", 3);

    int ret = ioctl(fd, PUSH_DATA, d);
    if (ret < 0) {
        perror("IOCTL failed");
    }

    close(fd);
    free(d->data);
    free(d);
    return ret;
}