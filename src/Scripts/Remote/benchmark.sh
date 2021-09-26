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
  echo -e "Config File: $CONFIG"
  echo -e "Config Test File: $TESTCONFIG"
  echo "------------------------------------------"
}

helpFunction()
{
   echo "Usage: sudo $0 -p 3 -n 20000 -l 32"
   echo -e "\t-n Number of PROPOSALS"
   echo -e "\t-p Number of PROCESSES"
   echo -e "\t-l Number of LANES"
   echo -e "\t-d Number of Disks"
   echo -e "\t-c Config File"
   echo -e "\t-m Main Replica"
   echo -e "\t-t Config Test File"
   exit 1 # Exit script after printing help
}

error_control()
{
  echo -e "\tERROR: $1 , Exiting"
  exit 1
}

while getopts "n:p:l:hd:c:m:t:" opt
do
   case "$opt" in
      n ) PROPOSALS="$OPTARG" ;;
      p ) PROCESSES="$OPTARG" ;;
      l ) LANES="$OPTARG" ;;
      d ) DISKS="$OPTARG" ;;
      c ) CONFIG="$OPTARG" ;;
      m ) MAIN_R="$OPTARG" ;;
      t ) TESTCONFIG="$OPTARG" ;;
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

if [ -z "$MAIN_R" ] || [ -z "$CONFIG" ]
then
  error_control 'Error on script arguments'
fi

set_up_enviroment(){
  cd thesis/Thesis/src/Scripts
  mkdir example_files
  echo -e "\t Generating a set of commands"
  (python3 ./gen_files.py $1 $2) || error_control 'Generating commands failed'
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

  return 0
}

print_config

echo -e "Setting up the enviroment\n"

jsonProcesses=$(jq -r '.processes' $CONFIG)
jsonlist=$(jq -r '.devices' $CONFIG)
        # inside the loop, you cant use the fuction _jq() to get values from each object.

for row in $(echo "${jsonProcesses}" | jq -r '.[] | @base64'); do
  #statements
  _jq()
  {
   echo ${row} | base64 --decode | jq -r ${1}
  }
  echo -e "\t Setting up the enviroment for $(_jq '.name')"
  echo -e "\t Generating a set of commands for each process"
  ssh $(_jq '.name') "$(typeset -f set_up_enviroment);$(typeset -f error_control); set_up_enviroment $PROPOSALS $PROCESSES"

  echo -e "\t Copying test configuration file"
  scp $TESTCONFIG $(_jq '.name'):thesis/Thesis/build/

  echo -e "\t Finished Setting up the enviroment for $(_jq '.name')\n"
done

echo -e "\nFinished Setting up the enviroment"
echo "------------------------------------------"

echo -e "Starting Disk Clean UP\n"

#PROCESSES LANES PROPOSALS nqn ip
reset_disk(){
  cd thesis/Thesis/build
  echo -e "\tReseting $4"
  (sudo ./Reset --processes $1 --lanes $2 --proposals $(($3 * 5)) --ip $5 --diskid $4 --port $6 --subnqn $4 &> logs/reset_cnode.log) || exit 1
  echo -e "\tReset on $4 was successful"
}

for row in $(echo "${jsonlist}" | jq -r '.[] | @base64'); do
  #statements
  _jq()
  {
   echo ${row} | base64 --decode | jq -r ${1}
  }

  ssh $MAIN_R "$(typeset -f reset_disk);$(typeset -f error_control); reset_disk $PROCESSES $LANES $PROPOSALS $(_jq '.nqn') $(_jq '.ip') $(_jq '.port')" || error_control "Cleaning Disk $(_jq '.nqn')"
done


echo -e "\nEnded Disk Clean UP"
echo "------------------------------------------"
echo -e "Launching consensus processes\n"

trids=$(jq -r '.trids' $CONFIG)

#PROCESSES LANES PID cpumask
diskpaxos_launch(){
  cd thesis/Thesis/build
  sudo ./DiskPaxos_SimpleProcess --processes $1 --lanes $2 --pid $3 --cpumask $4 --nvmf "$5" --config $6 &> logs/log_pid_$3.log
  return 0
}

for row in $(echo "${jsonProcesses}" | jq -r '.[] | @base64'); do
  #statements
  _jq()
  {
   echo ${row} | base64 --decode | jq -r ${1}
  }
  echo -e "\t Launching Process $(_jq '.id')"
  (ssh $(_jq '.name') "$(typeset -f diskpaxos_launch); diskpaxos_launch $PROCESSES $LANES $(_jq '.id') $(_jq '.cpumask') \"$trids\"" $(_jq '.config') && echo -e "\tProcess $(_jq '.id') exited successfully")&
done

echo -e "\nFinished Launching consensus processes"
echo -e "Waiting for the completion of the consensus"
wait
echo -e "Finished the test consensus"
echo "------------------------------------------"
echo -e "Analyzing results\n"

for row in $(echo "${jsonProcesses}" | jq -r '.[] | @base64'); do
  #statements
  _jq()
  {
   echo ${row} | base64 --decode | jq -r ${1}
  }
  scp -r $(_jq '.name'):thesis/Thesis/build/output .
done

ERROR=0
max=$(expr $PROCESSES - 1)
for i in `seq 0 $max`
do
  if ! cmp --silent <(sort -n output/output-0) <(sort -n output/output-$i) > /dev/null 2>&1
  then
    echo -e "\tResults of Processes 0 and $i differ"
    ERROR=$((ERROR + 1))
  else
    echo -e "\tResults of Processes 0 and $i are equal"
  fi
done
echo -e "\nFinished Analyzing results"
echo "------------------------------------------"
if [ $ERROR -ne 0 ];
then
  echo -e "\tTest ended with an error only $(expr $PROCESSES - $ERROR)/$PROCESSES Passed"
else
  echo -e "\tTest finished successfully, $PROCESSES/$PROCESSES are correct"
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

rm -rf output
