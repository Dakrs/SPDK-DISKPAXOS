#!/bin/sh

if command -v git >/dev/null 2>&1 ; then
    echo "git found"
else
    sudo apt-get install git
fi

if command -v pkg-config >/dev/null 2>&1 ; then
    echo "git found"
else
    sudo apt-get install pkg-config
fi

cd /home/diogosobral98

git clone https://github.com/spdk/spdk
cd spdk
git submodule update --init

sudo scripts/pkgdep.sh

./configure
make

./test/unit/unittest.sh
