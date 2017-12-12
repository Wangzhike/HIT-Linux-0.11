#!/bin/bash

make
cd ..
sudo ./mount-hdc
cd linux-0.11
cp iam.c whoami.c ../hdc/usr/root
cp testlab2.c testlab2.sh ../hdc/usr/root
cp include/unistd.h ../hdc/usr/include
cd ..
sudo umount hdc
./run
