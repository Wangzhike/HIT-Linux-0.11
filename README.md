# 操作系统代码实现及记录

**details:** 这是我在**网易云课堂**选的[操作系统](http://mooc.study.163.com/course/HIT-1000002004?tid=2001329005#/info)所要求的实验的代码及相关记录，授课老师是哈工大的李治军老师，李老师的讲课风格浅入深出，讲得非常好！我作为一个普通一本大学非计算机专业的学生，能接收到这样的教育，得感谢《网易云课堂》，感谢李治军老师。

**update:** 距离大三学习这门Mooc，已经过去两年了。现在我也算是如愿转到了计算机专业，目前来看自己最擅长的还是嵌入式领域，所以我想从之前停下来的地方开始，真正学好这门课，完成这八个实验，就像李老师所说，完成这八个实验，操作系统才算是入门了。希望自己早日完成这些实验，入门操作系统。
2017.03

## 0. 实验环境搭建
**致谢[DeathKing/hit-oslab项目](https://github.com/DeathKing/hit-oslab)**，提供了Ubuntu环境下的实验环境的一键配置脚本及其所需的源代码，工具等材料。我在此基础上进行了一些小改动，使配置脚本可以在Ubuntu 16.04上使用。具体参见[Linux 0.11实验环境准备](https://github.com/Wangzhike/HIT-Linux-0.11/blob/master/0-prepEnv/准备安装环境.md)。

## 1. 操作系统的引导
参见[操作系统的引导](https://github.com/Wangzhike/HIT-Linux-0.11/blob/master/1-boot/OS-booting.md)。

## 2. 系统调用    
参见[系统调用](https://github.com/Wangzhike/HIT-Linux-0.11/blob/master/2-syscall/2-syscall.md)

## 3. 进程运行轨迹的跟踪与统计
参见[进程运行轨迹的跟踪与统计](https://github.com/Wangzhike/HIT-Linux-0.11/blob/master/3-processTrack/3-processTrack.md)，修改时间片在我本地实验环境效果不明显，弃坑...，以后再研究是怎么回事:(

## 4. 基于内核栈切换的进程切换
参见[基于内核栈切换的进程切换](https://github.com/Wangzhike/HIT-Linux-0.11/blob/master/4-processSwitchWithKernelStack/4-processSwitchWithKernelStack.md)

## 5. 信号量的实现和应用    
参见[信号量的实现和应用](https://github.com/Wangzhike/HIT-Linux-0.11/blob/master/5-semaphore/5-semaphore.md)

## 6. 地址映射与共享

## 7. 终端设备的控制

## 8. proc文件系统的实现
