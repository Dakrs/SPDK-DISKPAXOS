#!/bin/sh

mkdir example_files

python3 ./gen_files.py 100 8

cp -r example_files ../../build

rm -r example_files

cd ../../build

if [ -d "output" ]; then
  rm -rf output #clean up old results
fi

mkdir output

make

sudo ./Reset --processes 8 --lanes 10 --proposals 1000 --local --diskid 0000:00:04.0-NS:1
sudo ./Reset --processes 8 --lanes 10 --proposals 1000 --local --diskid 0000:00:04.0-NS:2

sudo ./DiskPaxos_LocalThread 7 3 5 0xf

echo "====== Finished script ======"

echo "====== Results ======"

bash -c "diff <(sort -n output/output-7) <(sort -n output/output-3)"
bash -c "diff <(sort -n output/output-7) <(sort -n output/output-5)"
