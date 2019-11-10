#include <stdio.h>

int main()
{
    FILE* pFile;
    char buffer[17];
    int res;
    pFile = fopen("/dev/scull0", "r");
    res = fread(buffer, 1, 16, pFile);
    printf("Err: %d\n", res);
    printf("%s\n", buffer);
    return 0;
}
