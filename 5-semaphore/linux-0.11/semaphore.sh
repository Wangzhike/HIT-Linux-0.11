#!/bin/bash

# 编译内核
cd ~/oslab/linux-0.11
make
# 拷贝pc.c unistd.h sem.h myqueue.h到linux-0.11的文件系统hdc
cd ..
sudo ./mount-hdc
cd linux-0.11
cp pc.c ../hdc/usr/root
cp include/linux/sem.h include/linux/myqueue.h ../hdc/usr/include/linux
cp include/unistd.h ../hdc/usr/include
cd ..
sudo umount hdc
# 在bochs中运行内核
./run
# 运行结果，清理编译生成的中间文件，同时将运行pc.c的输出结果pc-out.txt拷贝回`~/oslab/linux-0.11`
cd ./linux-0.11
make clean
cd ..
sudo ./mount-hdc
sudo cp hdc/usr/root/pc-out.txt hdc/usr/root/pc-out-2.txt ./linux-0.11
sudo umount hdc

