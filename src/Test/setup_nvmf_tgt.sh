#!/bin/sh

SPDK_DIR=/home/gsd/spdk

#$SPDK_DIR/build/examples/nvmf -m 0xf -r /var/tmp/spdk.sock
#$SPDK_DIR/build/bin/nvmf_tgt -m $1 -c

helpFunction()
{
   echo ""
   echo "Usage: sudo $0 -m Ox2f -c ./config.json"
   echo "\t-m CPU mask what will be used"
   echo "\t-c Path to the configfile"
   exit 1 # Exit script after printing help
}

while getopts "m:c:" opt
do
   case "$opt" in
      m ) cpu_mask="$OPTARG" ;;
      c ) config="$OPTARG" ;;
      ? ) helpFunction ;; # Print helpFunction in case parameter is non-existent
   esac
done

# Print helpFunction in case parameters are empty
if [ -z "$cpu_mask" ] || [ -z "$config" ]
then
   echo "Some or all of the parameters are empty";
   helpFunction
fi

# Begin script in case all parameters are correct
$SPDK_DIR/build/bin/nvmf_tgt -m $cpu_mask -c $config
