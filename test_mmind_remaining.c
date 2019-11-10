#include <stdio.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#define MMIND_REMAINING _IOR('k', 13, int)

int main() {
    int fd;
    int remaining_guesses;

    fd = open("/dev/mastermind0", O_RDWR);

    if (fd < 0) {
        printf("CANNOT OPEN\n");  
        return -1;
    }

    ioctl(fd, MMIND_REMAINING, &remaining_guesses);
    printf("Remaining Guess Count: %d\n", remaining_guesses);

    return 0;
}