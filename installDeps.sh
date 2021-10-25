#!/bin/sh

helpFunction()
{
   echo ""
   echo "Usage: ./installDeps.sh -p ~/spdk"
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

if command -v git >/dev/null 2>&1 ; then
    echo "git found"
else
    sudo apt-get install -y git
fi

if command -v pkg-config >/dev/null 2>&1 ; then
    echo "pkg-config found"
else
    sudo apt-get install -y pkg-config
fi

if command -v bc >/dev/null 2>&1 ; then
    echo "bc found"
else
    sudo apt-get install -y bc
fi

if command -v jq >/dev/null 2>&1 ; then
    echo "jq found"
else
    sudo apt-get install -y jq
fi

sudo apt-get update

cd $SPDK_DIR

git clone https://github.com/spdk/spdk
cd spdk
git submodule update --init

sudo scripts/pkgdep.sh

./configure
make

./test/unit/unittest.sh
