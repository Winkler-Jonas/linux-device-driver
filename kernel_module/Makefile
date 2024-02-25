# this is a make file for a kernel object
# see online for more information

# will build "hello.ko"
ifneq ($(KERNELRELEASE),)
obj-m := rainbow_hat_driver.o

else
KDIR     := /lib/modules/$(shell uname -r)/build
PWD      := $(shell pwd)


default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

all:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules

modules_install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install


clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
endif