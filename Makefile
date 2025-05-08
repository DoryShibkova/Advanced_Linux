obj-m += int_stack.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	gcc -Wall -Wextra -o kernel_stack kernel_stack.c

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -f kernel_stack
