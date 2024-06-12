#!/usr/bin/env sh

idf.py build
mkdir build/encrypted
espsecure.py encrypt_flash_data --aes_xts --keyfile keys/flash.bin --address 0x8000 --output build/encrypted/parttab.bin build/partition_table/partition-table.bin
espsecure.py encrypt_flash_data --aes_xts --keyfile keys/flash.bin --address 0x10000 --output build/encrypted/app.bin build/undef_rfid.bin
esptool.py -b 3000000 write_flash --force 0x8000 build/encrypted/parttab.bin 0x10000 build/encrypted/app.bin
idf.py monitor
