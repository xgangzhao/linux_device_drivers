KERNEL_SRC="$(HOME)/linux/linux-source/WSL2-Linux-Kernel-linux-msft-wsl-6.6.87.2"

obj-m += globalfifo.o

build: kernel_modules

kernel_modules:
	make -C $(KERNEL_SRC) M=$(CURDIR) modules 

clean:
	make -C $(KERNEL_SRC) M=$(CURDIR) clean
