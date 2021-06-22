#!/bin/sh

mkdir example_files

python3 ./gen_files.py 40 8

cp -r example_files ../../build

rm -r example_files

cd ../../build

if [ -d "output" ]; then
  rm -rf output #clean up old results
fi

mkdir output

make

sudo ./Reset --processes 8 --lanes 10 --proposals 40 --diskid nqn.2016-06.io.spdk:cnode1 --port 4420 --subnqn nqn.2016-06.io.spdk:cnode1
sudo ./Reset --processes 8 --lanes 10 --proposals 40 --diskid nqn.2016-06.io.spdk:cnode2 --port 4421 --subnqn nqn.2016-06.io.spdk:cnode2

(sudo ./DiskPaxos_SimpleProcess --processes 8 --lanes 10 --pid 0 --cpumask 0x2) &
(sudo ./DiskPaxos_SimpleProcess --processes 8 --lanes 10 --pid 1 --cpumask 0x4) &
(sudo ./DiskPaxos_SimpleProcess --processes 8 --lanes 10 --pid 2 --cpumask 0x8) &

wait

echo "====== Finished script ======"

echo "====== Results ======"

bash -c "diff <(sort -n output/output-0) <(sort -n output/output-1)"
bash -c "diff <(sort -n output/output-0) <(sort -n output/output-2)"
