#!/bin/bash

# 编译内核
cd ~/oslab/linux-0.11
make
# 将process.c拷贝到linux-0.11的文件系统hdc
cd ..
sudo ./mount-hdc
cd linux-0.11
cp process.c ../hdc/usr/root
cd ..
sudo umount hdc

# 在bochs中运行内核
./run
# 运行结束，清理编译生成的中间文件，同时将process.log日志从hdc拷贝回~oslab/linux-0.11
cd ./linux-0.11
make clean
cd ..
sudo ./mount-hdc
cp hdc/var/process.log ./linux-0.11
sudo umount hdc

