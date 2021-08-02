#!/bin/sh

#$SPDK_DIR/build/examples/nvmf -m 0xf -r /var/tmp/spdk.sock
#$SPDK_DIR/build/bin/nvmf_tgt -m $1 -c

helpFunction()
{
   echo ""
   echo "Usage: sudo $0 -m 0x2f -c ./config.json"
   echo "\t-m CPU mask what will be used"
   echo "\t-c Path to the config file"
   echo "\t-p Path to spdk directory"
   echo "\t-d Debug mode"
   exit 1 # Exit script after printing help
}

while getopts "m:c:hp:d" opt
do
   case "$opt" in
      m ) cpu_mask="$OPTARG" ;;
      c ) config="$OPTARG" ;;
      p ) SPDK_DIR="$OPTARG" ;;
      d ) DEBUG="-e 0xFFFF" ;;
      h ) helpFunction ;;
      ? ) helpFunction ;; # Print helpFunction in case parameter is non-existent
   esac
done

if [ -z "$SPDK_DIR" ]
then
   echo "No Path to spdk given";
   helpFunction;
fi

# Print helpFunction in case parameters are empty
if [ -z "$cpu_mask" ] && [ -z "$config" ]
then
   echo "No parameters given";
   $SPDK_DIR/build/bin/nvmf_tgt $DEBUG
elif [ -z "$cpu_mask" ]
then
  echo "Config given"
  $SPDK_DIR/build/bin/nvmf_tgt -c $config $DEBUG
elif [ -z "$config" ]
then
  echo "CPU mask given"
  $SPDK_DIR/build/bin/nvmf_tgt -m $cpu_mask $DEBUG
else
  echo "Config and CPU mask given"
  $SPDK_DIR/build/bin/nvmf_tgt -m $cpu_mask -c $config $DEBUG
fi
