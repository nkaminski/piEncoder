obj-m += piEncoder.o
 
all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	gcc -o readencoder userland.c
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm piEncoder
