#!/bin/bash

mkdir /tmp/cgroup

cgroups=("cpu" "cpuset" "cpuacct" "memory" "blkio")
for cgroup in ${cgroups[@]};do
    echo "setup  $cgroup in /tmp/cgroup/$cgroup"
    if [ -d /tmp/cgroup/$cgroup ];then
        echo "cgroup $cgroup has mounted. exit"
    else
        sudo mkdir -p /tmp/cgroup/$cgroup
        sudo mount -t cgroup $cgroup -o $cgroup /tmp/cgroup/$cgroup
        sudo chown 1000:1000 -R /tmp/cgroup/$cgroup
    fi
done

sudo chown 1000:1000 -R /tmp/cgroup/cpu
sudo chmod 755 -R /tmp/cgroup/cpu

ls -la  /tmp/cgroup/cpu

mkdir bin

g++ -std=c++11 aucont_start.cpp common.h -o bin/aucont_start
g++ -std=c++11 aucont_stop.cpp common.h -o bin/aucont_stop
g++ -std=c++11 aucont_list.cpp common.h -o bin/aucont_list
g++ -std=c++11 aucont_exec.cpp common.h -o bin/aucont_exec