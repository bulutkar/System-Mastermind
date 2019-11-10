#include <stdio.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#define MMIND_REMAINING _IOR('k', 14, int)

int main() {
    int fd;
    int remaining_games;

    fd = open("/dev/scull0", O_RDWR);

    if (fd < 0) {
        printf("CANNOT OPEN");  
        return -1;
    }

    ioctl(fd, MMIND_REMAINING, &remaining_games);
    printf("Remaining Guess Count: %d\n", remaining_games);

    return 0;
}