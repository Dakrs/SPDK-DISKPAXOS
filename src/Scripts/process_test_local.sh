#!/bin/bash

print_config()
{
  echo -e "\tSTARTING AUTOMATED TEST"
  echo "------------------------------------------"
  echo -e "\tCURRENT CONFIG:"
  echo -e "Number of Proposals: $PROPOSALS"
  echo -e "Number of Processes: $PROCESSES"
  echo -e "Number of Lanes: $LANES"
  echo -e "Number of Disks: $DISKS"
  echo "------------------------------------------"
}

helpFunction()
{
   echo "Usage: sudo $0 -p 3 -n 20000 -l 32"
   echo -e "\t-n Number of PROPOSALS"
   echo -e "\t-p Number of PROCESSES"
   echo -e "\t-l Number of LANES"
   echo -e "\t-d Number of Disks"
   exit 1 # Exit script after printing help
}

error_control()
{
  echo -e "\tERROR: $1 , Exiting"
  exit 1
}

while getopts "n:p:l:hd:" opt
do
   case "$opt" in
      n ) PROPOSALS="$OPTARG" ;;
      p ) PROCESSES="$OPTARG" ;;
      l ) LANES="$OPTARG" ;;
      d ) DISKS="$OPTARG" ;;
      h ) helpFunction ;;
      ? ) helpFunction ;; # Print helpFunction in case parameter is non-existent
   esac
done

if [ -z "$PROPOSALS" ]
then
  PROPOSALS=20000
fi

if [ -z "$PROCESSES" ]
then
  PROCESSES=3
fi

if [ -z "$LANES" ]
then
  LANES=32
fi

if [ -z "$DISKS" ]
then
  DISKS=1
fi

print_config

echo -e "Setting up the enviroment\n"
echo -e "\t Generating a set of commands for each process"
mkdir example_files

(python3 ./gen_files.py $PROPOSALS $PROCESSES) || error_control 'Generating commands failed'

cp -r example_files ../../build

rm -r example_files

cd ../../build

if [ -d "output" ]; then
  echo -e "\t Removing old results"
  rm -rf output #clean up old results
fi

if [ -d "logs" ]; then
  echo -e "\t Removing old logs"
  rm -rf logs #clean up old results
fi

mkdir output
mkdir logs

echo -e "\t Compiling to the latest version"
make &> logs/make_output.log || error_control 'Compiling failed'
echo -e "\t Finished Compiling the latest version"
echo -e "\nFinished Setting up the enviroment"

echo "------------------------------------------"
echo -e "Starting Disk Clean UP\n"

max_disks=$(expr $DISKS - 1)
for i in `seq 0 $max_disks`
do
  echo -e "\tReseting nqn.2016-06.io.spdk:cnode$(expr $i + 1)"
  (sudo ./Reset --processes $PROCESSES --lanes $LANES --proposals $((PROPOSALS * 5)) --ip 127.0.0.1 --diskid nqn.2016-06.io.spdk:cnode$(expr $i + 1) --port $(expr $i + 1 + 4420) --subnqn nqn.2016-06.io.spdk:cnode$(expr $i + 1) &> logs/reset_cnode$(expr $i + 1).log) || error_control "Cleaning Disk cnode$(expr $i + 1)"
  echo -e "\tReset on nqn.2016-06.io.spdk:cnode$(expr $i + 1) was successful"
done

echo -e "\nEnded Disk Clean UP"
echo "------------------------------------------"
echo -e "Launching consensus processes\n"

max=$(expr $PROCESSES - 1)
for i in `seq 0 $max`
do
  echo -e "\t Launching Process $i"
  (sudo ./DiskPaxos_SimpleProcess --processes $PROCESSES --lanes $LANES --pid $i --nvmf "trtype:TCP adrfam:IPv4 traddr:127.0.0.1 trsvcid:4420 subnqn:nqn.2014-08.org.nvmexpress.discovery;" --cpumask 0x$(echo "2^($i+1)"| bc)) &> logs/log_pid_$i.log && echo -e "\tProcess $i exited successfully" &
done
echo -e "\nFinished Launching consensus processes"
echo -e "Waiting for the completion of the consensus"
wait
echo -e "Finished the test consensus"
echo "------------------------------------------"
echo -e "Analyzing results\n"

ERROR=0
max=$(expr $PROCESSES - 1)
for i in `seq 0 $max`
do
  cmp --silent <(sort -n output/output-0) <(sort -n output/output-$i) && echo -e "\tResults of Processes 0 and $i are equal" || (echo "\tResults of Processes 0 and $i differ"; ERROR++;)
done
echo -e "\nFinished Analyzing results"
echo "------------------------------------------"
if [ $ERROR -ne 0 ];
then
  echo -e "\tTest ended with an error"
else
  echo -e "\tTest finished successfully"
fi
echo "------------------------------------------"
echo -e "Test on final file length\n"
num_of_lines=$(wc -l < "output/output-0")
if [ $num_of_lines -eq $PROPOSALS ]
  then
    echo "Final file lenght is Correct"
  else
    echo "Final file lenght is Incorrect"
fi
echo "------------------------------------------"

: '
#sudo ./Reset --processes 8 --lanes 10 --proposals 6400 --diskid nqn.2016-06.io.spdk:cnode2 --port 4422 --subnqn nqn.2016-06.io.spdk:cnode2
#sudo ./Reset --processes 8 --lanes 10 --proposals 1600 --local --diskid 0000:00:04.0-NS:1
#sudo ./Reset --processes 8 --lanes 10 --proposals 1600 --local --diskid 0000:00:04.0-NS:2
'
