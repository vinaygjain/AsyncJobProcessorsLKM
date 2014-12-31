obj-m += sys_xjob.o
sys_xjob-objs := ./src/sys_xjob.o ./src/sys_xjob_encryption.o ./src/sys_xjob_netlink.o ./src/sys_xjob_checksum.o

all: admin xhw3 checksum encrypt compress xjob
 
admin: admin.c
	gcc -ggdb -Wall -Werror -I. admin.c -o admin

xhw3: xhw3.c
	gcc -ggdb -Wall -Werror -I. xhw3.c -o xhw3

checksum: user_checksum.c
	gcc -ggdb -Wall -Werror -I. user_checksum.c -o user_checksum

encrypt: user_encrypt.c
	gcc -ggdb -Wall -Werror -I. user_encrypt.c -o user_encrypt

compress: user_compress.c
	gcc -ggdb -Wall -Werror -I. user_compress.c -o user_compress

xjob:
	make -Wall -Werror -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -f xhw3 admin user_checksum user_encrypt user_compress
