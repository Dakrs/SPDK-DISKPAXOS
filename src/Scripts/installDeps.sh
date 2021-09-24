#!/bin/sh

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

cd /home/diogosobral98

sudo -u diogosobral98 git clone https://github.com/spdk/spdk
cd spdk
sudo -u diogosobral98 git submodule update --init

sudo scripts/pkgdep.sh

./configure
make

./test/unit/unittest.sh
