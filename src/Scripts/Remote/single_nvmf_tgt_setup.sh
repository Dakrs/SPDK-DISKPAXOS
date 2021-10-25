#!/bin/bash

helpFunction()
{
   echo ""
   echo "Usage: sudo $0 -p ~/spdk -d 0000:00:04:0 -n nqn.2016-06.io.spdk:cnode1 -i 35.225.240.68 -c 4420 -m 0x2"
   echo -e "\t-n NQN string"
   echo -e "\t-p Path to spdk directory"
   echo -e "\t-i Ip Address"
   echo -e "\t-c Port"
   echo -e "\t-d Local Disk Id"
   exit 1 # Exit script after printing help
}


while getopts "n:p:i:c:d:hm:" opt
do
   case "$opt" in
      n ) NQN="$OPTARG" ;;
      p ) SPDK_DIR="$OPTARG" ;;
      i ) IP="$OPTARG" ;;
      c ) PORT="$OPTARG" ;;
      d ) DISK_ID="$OPTARG" ;;
      m ) cpu_mask="$OPTARG" ;;
      h ) helpFunction ;;
      ? ) helpFunction ;; # Print helpFunction in case parameter is non-existent
   esac
done

if [ -z "$SPDK_DIR" ]
then
   echo "ERROR: No Path to spdk given";
   helpFunction;
fi

if [ -z "$DISK_ID" ]
then
   echo "ERROR: No Local Disk given";
   helpFunction;
fi

if [ -z "$IP" ]
then
   echo "ERROR: No IP given";
   helpFunction;
fi

if [ -z "$NQN" ]
then
   echo "ERROR: NQN String given";
   helpFunction;
fi

if [ -z "$PORT" ]
then
   echo "ERROR: Port given";
   helpFunction;
fi

if [ -z "$cpu_mask" ]
then
  cpu_mask=0x1
fi

mkdir /home/diogosobral98/thesis/Thesis/build/disk_log

cd $SPDK_DIR

sudo build/bin/nvmf_tgt -m $cpu_mask &> /home/diogosobral98/thesis/Thesis/build/disk_log/disk.log &

sleep 3

sudo scripts/rpc.py bdev_nvme_attach_controller -b nvme0 -a $DISK_ID -t pcie
sudo scripts/rpc.py nvmf_create_subsystem $NQN -a
sudo scripts/rpc.py nvmf_subsystem_add_ns $NQN nvme0n1
sudo scripts/rpc.py nvmf_create_transport -t tcp -q 16384
sudo scripts/rpc.py nvmf_subsystem_add_listener $NQN -t tcp -a $IP -s $PORT
