AsyncJobProcessorsLKM
=====================

Asynchronous job processors LKM

A producer-consumer work queue implemented as LKM. Users can submit Encryption(AES) and Checksum(CRC32) jobs. One producer and multiple consumers access the work queue and result queue synchronized by mutex. Results are communicated asynchronously (Signalling and NetLink).

Note: To use the LKM and the user programs, restructure the files in the following way:

$ ls -R
.:
kernel.config   xhw3.c          xhw3.h
admin.c         Makefile        user_compress.c  
src             user_encrypt.c  install_module.sh
user_checksum.c  

./src:
common.h                sys_xjob.c            sys_xjob_checksum.c    
sys_xjob_encryption.c   sys_xjob_netlink.c    sys_xjob.h


To compile and install the LKM:
$ make all && sh install_module.sh
