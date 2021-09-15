#!/bin/bash

helpFunction()
{
   echo ""
   echo "Usage: sudo $0 -p ~/spdk -n 4 -d 0000:00:04.0"
   echo -e "\t-n Number of Namespaces"
   echo -e "\t-p Path to spdk directory"
   echo -e "\t-d Local Disk Id"
   exit 1 # Exit script after printing help
}

while getopts "d:p:n:h" opt
do
   case "$opt" in
      n ) NUM_PROCESSES="$OPTARG" ;;
      d ) DISK_ID="$OPTARG" ;;
      p ) SPDK_DIR="$OPTARG" ;;
      h ) helpFunction ;;
      ? ) helpFunction ;; # Print helpFunction in case parameter is non-existent
   esac
done

if [ -z "$SPDK_DIR" ]
then
   echo "ERROR: No Path to spdk given";
   helpFunction;
fi


re='^[0-9]+$'
if ! [[ $NUM_PROCESSES =~ $re ]] ;
then
   echo "ERROR: NUM_PROCESSES is not a number or was not given" >&2;
   helpFunction;
fi


if [ -z "$DISK_ID" ]
then
   echo "ERROR: No Local Disk given";
   helpFunction;
fi

cd $SPDK_DIR

sudo ./scripts/rpc.py bdev_nvme_attach_controller -b nvme0 -a 0000:00:04.0 -t pcie

for i in `seq 1 $NUM_PROCESSES`
do
  sudo ./scripts/rpc.py nvmf_create_subsystem nqn.2016-06.io.spdk:cnode$i -a
  sudo ./scripts/rpc.py nvmf_subsystem_add_ns nqn.2016-06.io.spdk:cnode$i nvme0n$i
done

sudo ./scripts/rpc.py nvmf_create_transport -t tcp -q 16384
sudo ./scripts/rpc.py nvmf_subsystem_add_listener nqn.2014-08.org.nvmexpress.discovery -t tcp -a 127.0.0.1 -s 4420

for i in `seq 1 $NUM_PROCESSES`
do
  sudo ./scripts/rpc.py nvmf_subsystem_add_listener nqn.2016-06.io.spdk:cnode$i -t tcp -a 127.0.0.1 -s $(expr $i + 4420)
done
