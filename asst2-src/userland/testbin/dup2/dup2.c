#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <err.h>
#include <errno.h>

#define MAX_BUF 500
char teststr[] = "test 222";
char buf[MAX_BUF];

int main(void) {
        int fd, r, i, j , k;
        //off_t rOff;
        printf("**********\n* opening new file \"test.file\"\n");
        fd = open("test.file", O_RDWR | O_CREAT, 0700); /* mode u=rw in octal */
        printf("* open() got fd %d\n", fd);
        if (fd < 0) {
                printf("ERROR opening file: %s\n", strerror(errno));
                exit(1);
        }

        printf("* writing test string\n");
        r = write(fd, teststr, strlen(teststr));
        printf("* wrote %d bytes\n", r);
        if (r < 0) {
                printf("ERROR writing file: %s\n", strerror(errno));
                exit(1);
        }

        printf("* initialising dup2\n");
        int new_fd = 8;
        r = dup2(fd, new_fd);
        printf("* dup2 on %d\n", r);
        if (r < 0) {
                printf("ERROR dup2: %s\n", strerror(errno));
                exit(1);
        }

        printf("* writing test string using new dup2 fd\n");
        r = write(new_fd, teststr, strlen(teststr));
        printf("* wrote %d bytes\n", r);
        if (r < 0) {
                printf("ERROR writing file: %s\n", strerror(errno));
                exit(1);
        }

        printf("* reading entire file into buffer \n");
        i = 0;
        do  {
                printf("* attempting read of %d bytes\n", MAX_BUF -i);
                r = read(fd, &buf[i], MAX_BUF - i);
                printf("* read %d bytes\n", r);
                i += r;
        } while (i < MAX_BUF && r > 0);

        k = j = 0;
        r = strlen(teststr);
        do {
                if (buf[k] != teststr[j]) {
                        printf("ERROR  file contents mismatch\n");
                        exit(1);
                }
                k++;
                j = k % r;
        } while (k < i);
        printf("* file content okay\n");

        printf("* closing original file\n");
        close(fd);

        printf("* reading entire file into buffer - file should still be open\n");
        i = 0;
        do  {
                printf("* attempting read of %d bytes\n", MAX_BUF -i);
                r = read(new_fd, &buf[i], MAX_BUF - i);
                printf("* read %d bytes\n", r);
                i += r;
        } while (i < MAX_BUF && r > 0);

        printf("* reading complete\n");
        if (r < 0) {
                printf("ERROR reading file: %s\n", strerror(errno));
                exit(1);
        }

        printf("* closing new file\n");
        close(new_fd);



        // test error cases
        printf("* testing dup2 - no open file\n");
        r = dup2(9, 10);
        if (r < 0) {
                printf("ERROR dup2: %s\n", strerror(errno));
        }
        else {
                printf("* dup2 should return error\n");
                exit(1);
        }

        printf("* testing dup2 - invalid fd\n");
        r = dup2(-10, 10);
        if (r < 0) {
                printf("ERROR dup2: %s\n", strerror(errno));
        }
        else {
                printf("* dup2 should return error\n");
                exit(1);
        }


        return 0;
}