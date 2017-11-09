#!/bin/bash

cd ~/oslab/linux-0.11
make
cd ..
./run
cd ./linux-0.11
make clean
