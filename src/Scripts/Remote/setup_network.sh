#!/bin/bash

jsonlist=$(jq -r '.devices' $1)
        # inside the loop, you cant use the fuction _jq() to get values from each object.

for row in $(echo "${jsonlist}" | jq -r '.[] | @base64'); do
  #statements
  _jq()
  {
   echo ${row} | base64 --decode | jq -r ${1}
  }

  sudo ./single_nvmf_tgt_setup.sh -p /home/diogosobral98/spdk -d $(_jq '.diskid') -n $(_jq '.nqn') -i $(_jq '.ip') -c $(_jq '.port') -m $(_jq '.cpumask')
done
