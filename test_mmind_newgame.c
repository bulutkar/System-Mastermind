#include <stdio.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#define MMIND_NEWGAME _IOW('k', 15, int)

int main() {
    int fd;
    int new_secret_number = 5566;

    fd = open("/dev/mastermind0", O_RDWR);

    if (fd < 0) {
        printf("CANNOT OPEN\n");  
        return -1;
    }

    printf("Calling MMIND_NEWGAME ioctl command.\n");
    ioctl(fd, MMIND_NEWGAME, &new_secret_number);
    printf("Secret number changed. New game started.\n");


    return 0;
}

