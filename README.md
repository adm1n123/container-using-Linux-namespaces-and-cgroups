# container-using-Linux-namespaces-and-cgroups
CS-695   Programming Assignment 3: Build your own container using Linux namespaces and cgroups

#### use root file system 
https://github.com/ericchiang/containers-from-scratch/releases/download/v0.1.0/rootfs.tar.gz


### Configure network
#### Create two network namespaces(netns-contnr1 and netns-contnr2)
$ ./network_config


### create client(set server IP) and server executables
$ make client
$ make server
move the server executable into root file system


### compile container
$ make container


### run container args(root file system, hostname, container id)
$ ./container rootfs1 my-container1 contnr1


### start server process from container shell
$ ./server &


### start client from host and communicate.
$ ./client


### create another container 
#### only change server IP in client.c, use different hostname, and container id: contnr2
$ make client
$ ./container rootfs2 my-container2 contnr2

