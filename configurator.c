#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define DRIVER_NAME "/dev/vicharak"
#define SET_SIZE_OF_QUEUE _IOW('a', 'a', int)

int main(void) {
    int fd = open(DRIVER_NAME, O_RDWR);
    if (fd < 0) {
        perror("Failed to open device");
        return -1;
    }

    int size = 100;
    int ret = ioctl(fd, SET_SIZE_OF_QUEUE, &size);
    if (ret < 0) {
        perror("IOCTL failed");
    }

    close(fd);
    return ret;
}