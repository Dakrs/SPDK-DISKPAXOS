#include "Processes/Leader.hpp"
#include "Processes/Replica.hpp"
#include "Processes/MultiReplica.hpp"
#include "Disk/SPDK_ENV.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <thread>

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>

#define no_argument 0
#define required_argument 1
#define optional_argument 2

void helpFunction();
bool verify_args(int N_PROCESSES, int N_LANES, int PID, std::string cpumask);

void helpFunction()
{
   std::cout << "Usage: sudo ./DiskPaxos_SimpleProcess --processes 8 --lanes 10 --pid 1 -b" << std::endl;
   std::cout << "\t-n or --processes specifies the numbers of processes REQUIRED" << '\n';
   std::cout << "\t-l or --lanes specifies the numbers of lanes REQUIRED" << '\n';
   std::cout << "\t-i or --pid specifies the process identifier, must be between 0 and N_PROCESSES - 1 REQUIRED" << '\n';
   std::cout << "\t-b or --local indicates that a local device should be used" << '\n';
   std::cout << "\t-m or --cpumask specifies the cores that will be used REQUIRED" << '\n';
}

bool verify_args(int N_PROCESSES, int N_LANES, int PID, std::string cpumask){
  if (N_PROCESSES == -1){
    std::cout << "Error: Option -n or --processes must be specified" << std::endl;
    return false;
  }

  if (N_LANES == -1 || N_LANES <= 0){
    std::cout << "Error: Option -l or --lanes must be specified" << std::endl;
    return false;
  }

  if (PID == -1){
    std::cout << "Error: Option -p or --proposals must be specified" << std::endl;
    return false;
  }

  if (PID < 0 || PID >= N_PROCESSES){
    std::cout << "Error: PID must be an integer between 0 and N_PROCESSES - 1" << std::endl;
    return false;
  }

  if (cpumask == ""){
    std::cout << "Error: Option -m or --cpumask must be specified" << std::endl;
    return false;
  }

  return true;
}

int main(int argc, char *argv[]) {
  int N_PROCESSES = -1,N_LANES = -1,PID = -1;
  bool local = false;

  std::string cpumask = "";

  const struct option longopts[] =
  {
    {"processes",required_argument,0, 'n'},
    {"lanes",required_argument,0,'l'},
    {"pid",required_argument,0, 'i'},
    {"local",no_argument,0, 'b'},
    {"help",no_argument,0,'h'},
    {"cpumask",required_argument,0,'m'},
    {0,0,0,0},
  };

  int option_index = 0;
  int c;
  while (1) {
    c = getopt_long(argc, argv, "n:l:bi:h",longopts, &option_index);
    if (c == -1){
      break;
    }

    switch (c) {
      case 0:
          break;
      case 'n':
          N_PROCESSES = atoi(optarg);
          std::cout << "N_PROCESSES: " << N_PROCESSES << std::endl;
          break;
      case 'l':
          N_LANES = atoi(optarg);
          std::cout << "N_LANES: " << N_LANES << std::endl;
          break;
      case 'i':
          PID = atoi(optarg);
          std::cout << "PID: " << PID << std::endl;
          break;
      case 'b':
          local = true;
          std::cout << "Using local device" << std::endl;
          break;
      case 'm':
          cpumask = std::string(optarg);
          std::cout << "Using CPU Mask: " << cpumask << std::endl;
          break;
      case 'h':
          helpFunction();
          exit(1);
          break;
      case '?':
        break;
      default :
        break;
    }
  }

  if(!verify_args(N_PROCESSES,N_LANES,PID,cpumask)){
      helpFunction();
      exit(-1);
  }

  //std::vector<std::string> example {"trtype:TCP adrfam:IPv4 traddr:127.0.0.1 trsvcid:4420 subnqn:nqn.2016-06.io.spdk:cnode1"};
  const char * c_mask = cpumask.c_str();

  if (local){
    SPDK_ENV::spdk_start(N_PROCESSES,N_LANES,c_mask);
  }
  else{
    std::vector<std::string> trids {
      "trtype:TCP adrfam:IPv4 traddr:127.0.0.1 trsvcid:4420 subnqn:nqn.2014-08.org.nvmexpress.discovery"
    };
    SPDK_ENV::spdk_start(N_PROCESSES,N_LANES,c_mask,trids);
  }


  LeaderPaxos::LeaderPaxos lp(PID,N_LANES);
  std::thread leader_thread(&LeaderPaxos::LeaderPaxos::run,&lp);
  MultiReplicaPaxos::MultiReplicaPaxos rp(PID,N_LANES);
  rp.run();
  leader_thread.join();
  SPDK_ENV::spdk_end();

  std::cout << "Process with pid: " << PID << " quiting" << std::endl;

  return 0;
}
