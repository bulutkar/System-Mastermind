# System-Mastermind
It is character device driver module that will be play the board game "Master Mind".

How to use:
- make
- sudo su
- insmod ./mastermind.ko mmind_number="1234" mmind_max_guesses=25 (mmind_max_guesses is optional, default = 10)
- mknod /dev/mastermind0 c 250 0 (assuming major number is 250, can be checked with "grep mastermind /proc/devices")
- echo "4321" > /dev/mastermind0  (guess the secret number)
- cat /dev/mastermind0            (read guess history)

IOCTL Test Files:
3 test files for 3 ioctl commands (MMIND_REMAINING, MMIND_ENDGAME, MMIND_NEWGAME.

- gcc test_mmind_remaining.c -o remaining
- ./remaining
- Program will print remaining guess count by calling MMIND_REMAINING ioctl command.

- gcc test_mmind_endgame.c -o endgame
- ./endgame
- Program will print remaining guess count before MMIND_ENDGAME,
then program will call MMIND_ENDGAME to clear guess history and guess count,
then program will print remaining guess count again to show history is cleared.

- gcc test_mmind_newgame.c -o newgame
- ./newgame
- Program will ask player to enter a new secret number,
then program will change secret number and start a new game by calling MMIND_NEWGAME ioctl command.
