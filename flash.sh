#!/usr/bin/env sh

idf.py build
espsecure.py encrypt_flash_data --aes_xts --keyfile flash_key.bin --address 0x10000 --output build/encrypted_app.bin build/undef_rfid.bin
esptool.py -b 2000000 write_flash --force 0x10000 build/encrypted_app.bin
idf.py monitor
