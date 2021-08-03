#!/bin/sh

PROPOSALS=40000

mkdir example_files

python3 ./gen_files.py $PROPOSALS 3

cp -r example_files ../../build

rm -r example_files

cd ../../build

if [ -d "output" ]; then
  rm -rf output #clean up old results
fi

mkdir output

make

sudo ./Reset --processes 3 --lanes 32 --proposals $((PROPOSALS * 5)) --diskid nqn.2016-06.io.spdk:cnode1 --port 4421 --subnqn nqn.2016-06.io.spdk:cnode1
sudo ./Reset --processes 3 --lanes 32 --proposals $((PROPOSALS * 5)) --diskid nqn.2016-06.io.spdk:cnode2 --port 4422 --subnqn nqn.2016-06.io.spdk:cnode2
sudo ./Reset --processes 3 --lanes 32 --proposals $((PROPOSALS * 5)) --diskid nqn.2016-06.io.spdk:cnode3 --port 4423 --subnqn nqn.2016-06.io.spdk:cnode3

#sudo ./Reset --processes 8 --lanes 10 --proposals 6400 --diskid nqn.2016-06.io.spdk:cnode2 --port 4422 --subnqn nqn.2016-06.io.spdk:cnode2
#sudo ./Reset --processes 8 --lanes 10 --proposals 1600 --local --diskid 0000:00:04.0-NS:1
#sudo ./Reset --processes 8 --lanes 10 --proposals 1600 --local --diskid 0000:00:04.0-NS:2

(sudo ./DiskPaxos_SimpleProcess --processes 3 --lanes 32 --pid 0 --cpumask 0x2) &
#(sudo ./DiskPaxos_SimpleProcess --processes 3 --lanes 32 --pid 1 --cpumask 0x4) &
#(sudo ./DiskPaxos_SimpleProcess --processes 3 --lanes 32 --pid 2 --cpumask 0x8) &

wait

echo "====== Finished script ======"

echo "====== Results ======"

#bash -c "diff <(sort -n output/output-0) <(sort -n output/output-1)"
#bash -c "diff <(sort -n output/output-0) <(sort -n output/output-2)"

echo "======= Length Test ========"
num_of_lines=$(< "output/output-0" wc -l)

if [ $num_of_lines -eq $PROPOSALS ]
  then
    echo "Number of consensus is Correct"
  else
    echo "Number of consensus is Incorrect"
fi
