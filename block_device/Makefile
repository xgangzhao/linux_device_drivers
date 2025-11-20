KERNEL_SRC="$(HOME)/linux/linux-source/WSL2-Linux-Kernel-linux-msft-wsl-6.6.87.2"

obj-m += sbull.o

modules:
	make -C $(KERNEL_SRC) M=$(CURDIR) modules 

clean:
	make -C $(KERNEL_SRC) M=$(CURDIR) clean
