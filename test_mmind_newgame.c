/*
** Group ID: 12
** Bulut KARABIYIK - 150150048
** Alperen METINTAS - 150150025
** Zeynep YETISTIREN - 150150020
*/

#include <stdio.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#define MMIND_NEWGAME _IOW('k', 15, int)

int main() {
    int fd;
    int new_secret_number;

    fd = open("/dev/mastermind0", O_RDWR);

    if (fd < 0) {
        printf("CANNOT OPEN\n");  
        return -1;
    }

    printf("Calling MMIND_NEWGAME ioctl command.\n");
    printf("Plase enter new secret number: ");
    scanf("%d", &new_secret_number);
    ioctl(fd, MMIND_NEWGAME, &new_secret_number);
    printf("Secret number changed. New game started.\n");


    return 0;
}

