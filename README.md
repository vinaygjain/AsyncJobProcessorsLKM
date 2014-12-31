AsyncJobProcessorsLKM
=====================

Asynchronous job processors LKM

A producer-consumer work queue implemented as LKM. Users can submit Encryption(AES) and Checksum(CRC32) jobs. One producer and multiple consumers access the work queue and result queue synchronized by mutex. Results are communicated asynchronously (Signalling and NetLink).
