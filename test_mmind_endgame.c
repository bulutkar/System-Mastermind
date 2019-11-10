#include <stdio.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#define MMIND_REMAINING _IOR('k', 14, int)
#define MMIND_ENDGAME _IO('k', 15)

int main() {
    int fd;
    int remaining_guesses;

    fd = open("/dev/scull0", O_RDWR);

    if (fd < 0) {
        printf("CANNOT OPEN");  
        return -1;
    }

    ioctl(fd, MMIND_REMAINING, &remaining_guesses);
    printf("Remaining guesses before MMIND_ENDGAME: %d\n", remaining_guesses);

    printf("Calling MMIND_ENDGAME ioctl command.\n");
    ioctl(fd, MMIND_ENDGAME);

    ioctl(fd, MMIND_REMAINING, &remaining_guesses);
    printf("Remaining guesses after MMIND_ENDGAME: %d\n", remaining_guesses);

    return 0;
}