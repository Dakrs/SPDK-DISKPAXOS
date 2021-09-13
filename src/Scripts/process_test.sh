#!/bin/bash

PROPOSALS=20000
PROCESSES=3

mkdir example_files

python3 ./gen_files.py $PROPOSALS 3

cp -r example_files ../../build

rm -r example_files

cd ../../build

if [ -d "output" ]; then
  rm -rf output #clean up old results
fi

if [ -d "logs" ]; then
  rm -rf logs #clean up old results
fi

mkdir output
mkdir logs

make

echo "====== Starting Disk Clean UP ======"

sudo ./Reset --processes 3 --lanes 32 --proposals $((PROPOSALS * 5)) --diskid nqn.2016-06.io.spdk:cnode1 --port 4421 --subnqn nqn.2016-06.io.spdk:cnode1 > logs/reset_cnode1.log
#sudo ./Reset --processes 3 --lanes 32 --proposals $((PROPOSALS * 5)) --diskid nqn.2016-06.io.spdk:cnode2 --port 4422 --subnqn nqn.2016-06.io.spdk:cnode2
#sudo ./Reset --processes 3 --lanes 32 --proposals $((PROPOSALS * 5)) --diskid nqn.2016-06.io.spdk:cnode3 --port 4423 --subnqn nqn.2016-06.io.spdk:cnode3

#sudo ./Reset --processes 8 --lanes 10 --proposals 6400 --diskid nqn.2016-06.io.spdk:cnode2 --port 4422 --subnqn nqn.2016-06.io.spdk:cnode2
#sudo ./Reset --processes 8 --lanes 10 --proposals 1600 --local --diskid 0000:00:04.0-NS:1
#sudo ./Reset --processes 8 --lanes 10 --proposals 1600 --local --diskid 0000:00:04.0-NS:2

echo "====== Ending Disk Clean UP ======"

echo "====== Starting Test Script ======"

(sudo ./DiskPaxos_SimpleProcess --processes 3 --lanes 32 --pid 0 --cpumask 0x2) > logs/log_pid_0.log 2>&1 &
(sudo ./DiskPaxos_SimpleProcess --processes 3 --lanes 32 --pid 1 --cpumask 0x4) > logs/log_pid_1.log 2>&1 &
(sudo ./DiskPaxos_SimpleProcess --processes 3 --lanes 32 --pid 2 --cpumask 0x8) > logs/log_pid_2.log 2>&1 &

wait

echo "====== Finished Test Script ======"

echo "====== Results ======"

max=$(expr $PROCESSES - 1)
for i in `seq 0 $max`
do
  cmp --silent <(sort -n output/output-0) <(sort -n output/output-$i) && echo "Results of Processes 0 and $i are equal" || echo "Results of Processes 0 and $i differ"
done

#cmp --silent <(sort -n output/output-0) <(sort -n output/output-1) <(sort -n output/output-2) && echo "Results are identical" || echo "files are different"

#bash -c "diff <(sort -n output/output-0) <(sort -n output/output-1)"
#bash -c "diff <(sort -n output/output-0) <(sort -n output/output-2)"

echo "======= Length Test ========"
num_of_lines=$(wc -l < "output/output-0")

if [ $num_of_lines -eq $PROPOSALS ]
  then
    echo "Number of consensus is Correct"
  else
    echo "Number of consensus is Incorrect"
fi
