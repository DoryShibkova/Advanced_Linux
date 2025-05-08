#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>

#define DEVICE_PATH "/dev/int_stack"
#define IOCTL_SET_SIZE _IOW('s', 1, int)

// Display usage instructions
void print_usage() {
    printf("Usage:\n");
    printf("  kernel_stack set-size <size>\n");
    printf("  kernel_stack push <value>\n");
    printf("  kernel_stack pop\n");
    printf("  kernel_stack unwind\n");
}

// Main program entry point
int main(int argc, char *argv[]) {
    int fd;
    int value;
    int ret;

    // Check if the device file exists
    if (access(DEVICE_PATH, F_OK) == -1) {
        fprintf(stderr, "error: USB key not inserted\n");
        return 1;
    }

    if (argc < 2) {
        print_usage();
        return 1;
    }

    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("Failed to open device");
        return 1;
    }

    if (strcmp(argv[1], "set-size") == 0) {
        if (argc != 3) {
            print_usage();
            close(fd);
            return 1;
        }
        
        value = atoi(argv[2]);
        if (value <= 0) {
            printf("ERROR: size should be > 0\n");
            close(fd);
            return 1;
        }
        
        ret = ioctl(fd, IOCTL_SET_SIZE, &value);
        if (ret < 0) {
            perror("ERROR: failed to set stack size");
            close(fd);
            return -ret;  // Return negative error code
        }
    }
    else if (strcmp(argv[1], "push") == 0) {
        if (argc != 3) {
            print_usage();
            close(fd);
            return 1;
        }
        
        value = atoi(argv[2]);
        ret = write(fd, &value, sizeof(int));
        if (ret < 0) {
            if (errno == ERANGE) {
                printf("ERROR: stack is full\n");
                close(fd);
                return -ERANGE;  // Return -34 for stack full
            } else {
                perror("Failed to push value");
                close(fd);
                return -errno;  // Return negative error code
            }
        }
    }
    else if (strcmp(argv[1], "pop") == 0) {
        if (argc != 2) {
            print_usage();
            close(fd);
            return 1;
        }
        
        ret = read(fd, &value, sizeof(int));
        if (ret == 0) {
            printf("NULL\n");
            close(fd);
            return 0;  // Return 0 for empty stack
        } else if (ret < 0) {
            perror("Failed to pop value");
            close(fd);
            return -errno;  // Return negative error code
        } else {
            printf("%d\n", value);
        }
    }
    else if (strcmp(argv[1], "unwind") == 0) {
        if (argc != 2) {
            print_usage();
            close(fd);
            return 1;
        }
        
        while (1) {
            ret = read(fd, &value, sizeof(int));
            if (ret == 0) {
                break;  // Stack is empty
            } else if (ret < 0) {
                perror("Failed to unwind stack");
                close(fd);
                return -errno;  // Return negative error code
            } else {
                printf("%d\n", value);
            }
        }
    }
    else {
        print_usage();
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}