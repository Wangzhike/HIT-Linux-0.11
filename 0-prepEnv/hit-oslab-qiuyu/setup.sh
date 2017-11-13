#!/bin/sh
export OSLAB_INSTALL_PATH=$HOME

install_gcc34() {
    echo -n "* Install gcc-3.4 for x86(i386) arch now......"
    if [ -z `which gcc-3.4` ]; then   #如果搜索到gcc-3.4的可执行文件目录不存在
        tar zvxf gcc-3.4-ubuntu.tar.gz -C /tmp    #则安装gcc-3.4
        cd /tmp/gcc-3.4
        sudo ./inst.sh amd64
        cd -
        echo "\033[34mDone\033[0m"
    else
        echo "\033[33mSkipped\033[0m"
    fi
}

install_dep_i386() {    #安装需要的32位依赖文件
    echo "* Install x86(i386) dependencies for x86_64(amd64) arch now......"
    # Install bin86
    echo -n "* Install bin86 that includes as86 and ld86 for compiling and linking bootsect.s and setup.s"
    sudo apt-get install bin86
    echo "\033[34mDone\033[0m"

    # Install 32bit libs
    echo -n "* Install 32bit libs"
    sudo apt-get install libc6-dev-i386
    echo "\033[34mDone\033[0m"

    # Install compilation environment for C
    echo -n "* Install compilation environment for C"
    sudo apt-get install build-essential
    echo "\033[34mDone\033[0m"

    # Install libSM:i386 for bochs
    echo -n "* Install libSM:i386 for bochs"
    sudo apt-get install libsm6:i386
    echo "\033[34mDone\033[0m"

    # Install libX11-6:i386 for bochs
    echo -n "* Install libX11-6:i386 for bochs"
    sudo apt-get install libx11-6:i386
    echo "\033[34mDone\033[0m"

    # Install libxpm4:i386 for bochs
    echo -n "* Install libxpm4:i386 for bochs"
    sudo apt-get install libxpm4:i386
    echo "\033[34mDone\033[0m"
    echo "* Install x86(i386) dependencies for x86_64(amd64) arch finished \033[34mDone\033[0m"
}

# Common Code
if [ "$1" ] && ([ "$1" = "-s" ] || [ "$1" = "--skip-update" ]); then
    echo -n "* Begin to setup......\033[33m3\033[0m sec to start"; sleep 1
    echo -n "\r* Begin to setup......\033[33m2\033[0m sec to start"; sleep 1
    echo -n "\r* Begin to setup......\033[33m1\033[0m sec to start"; sleep 1
    echo "\r* Begin to setup......                                 \033[0m"
else
    echo -n "* Update apt sources......\033[33m3\033[31m sec to start"; sleep 1
    echo -n "\r* Update apt sources......\033[33m2\033[31m sec to start"; sleep 1
    echo -n "\r* Update apt sources......\033[33m1\033[31m sec to start"; sleep 1
    echo "\r* Update apt sources......                                 "
    sudo apt-get update
fi

echo -n "* Create oslab main directory......"
[ -d $OSLAB_INSTALL_PATH ] || mkdir -p $OSLAB_INSTALL_PATH  #如果安装目录存在则跳过，如果安装目录不存在则新建安装目录
echo "\033[34mDone\033[0m"

# Extract linux-0.11 and bochs and hdc image
echo -n "* Extract linux-0.11 and bochs and hdc image......"
tar zxf hit-oslab-linux-qiuyu.tar.gz -C $OSLAB_INSTALL_PATH
echo "\033[34mDone\033[0m"

# Extract and Install gcc-3.4
install_gcc34

#Install x86(i386) dependencies for x86_64(amd64) arch
install_dep_i386

echo "\033[34m* Installation finished.\033[0m"
