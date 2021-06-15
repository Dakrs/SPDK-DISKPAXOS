#!/bin/sh

mkdir example_files

python3 ./gen_files.py 40 8

cp -r example_files ../../build

rm -r example_files

cd ../../build

mkdir output

make

sudo ./Reset --processes 8 --lanes 10 --proposals 40 --local --diskid 0000:03:00.0

sudo ./DiskPaxos_LocalThread 0 1 2

echo "====== Finished script ======"

echo "====== Results ======"

bash -c "diff <(sort -n output/output-0) <(sort -n output/output-1)"
bash -c "diff <(sort -n output/output-0) <(sort -n output/output-2)"
