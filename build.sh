#!/bin/sh

helpFunction()
{
   echo ""
   echo "Usage: ./build -p ~/spdk"
   echo "\t-p Path to spdk directory"
   exit 1 # Exit script after printing help
}

while getopts "p:h" opt
do
   case "$opt" in
      p ) SPDK_DIR="$OPTARG" ;;
      h ) helpFunction ;;
      ? ) helpFunction ;; # Print helpFunction in case parameter is non-existent
   esac
done

if [ -z "$SPDK_DIR" ]
then
   echo "No Path to spdk given";
   helpFunction;
fi

if [ -d "build" ]; then
  rm -rf build
fi

mkdir -p build
cd build
cmake .. -DSPDK_P=$SPDK_DIR
cmake --build .
