#include "Processes/Leader.hpp"
#include "Processes/Replica.hpp"
#include "Processes/MultiReplica.hpp"
#include "Disk/SPDK_ENV.hpp"
#include "Disk/SglOpts.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <thread>

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#define no_argument 0
#define required_argument 1
#define optional_argument 2

void helpFunction();
bool verify_args(int N_PROCESSES, int N_LANES, int PID, std::string cpumask);
void compute_trids(std::string str, std::vector<std::string> * trids);
void setConfig(std::string config_file,std::string * app_name,int * qpair_queue_size,int * qpair_queue_request,int * proposal_interval,int * read_amount_replica,int * number_of_slots_to_read);


void helpFunction()
{
   std::cout << "Usage: sudo ./DiskPaxos_SimpleProcess --processes 8 --lanes 10 --pid 1 -b" << std::endl;
   std::cout << "\t-n or --processes specifies the numbers of processes REQUIRED" << std::endl;
   std::cout << "\t-l or --lanes specifies the numbers of lanes REQUIRED" << std::endl;
   std::cout << "\t-i or --pid specifies the process identifier, must be between 0 and N_PROCESSES - 1 REQUIRED" << std::endl;
   std::cout << "\t-b or --local indicates that a local device should be used" << std::endl;
   std::cout << "\t-m or --cpumask specifies the cores that will be used REQUIRED" << std::endl;
   std::cout << "\t-s or --nvmf specifies NQN string to connect to remote disks" << std::endl;
   std::cout << "\t-c or --config JSON File with the configuration to the test to run" << std::endl;
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

void compute_trids(std::string str, std::vector<std::string> * trids){
  std::string delimiter = ";";
  size_t pos = 0;
  std::string token;

  while ((pos = str.find(delimiter)) != std::string::npos) {
      token = str.substr(0, pos);
      trids->push_back(token);
      str.erase(0, pos + delimiter.length());
  }
}

void setConfig(std::string config_file_str,std::string * app_name,int * qpair_queue_size,int * qpair_queue_request,int * proposal_interval,int * read_amount_replica,int * number_of_slots_to_read){
  /**
  std::ifstream config_file(config_file_str, std::ifstream::binary);
  config_file >> config;

  cout << config;*/

  std::ifstream config_file(config_file_str);
  json config;
  config_file >> config;

  if (config["name"] != NULL){
    *app_name = config["name"].get<std::string>();
  }

  if (config["qpair_queue_size"] != NULL){
    *qpair_queue_size = stoi(config["qpair_queue_size"].get<std::string>());
  }

  if (config["qpair_queue_request"] != NULL){
    *qpair_queue_request = stoi(config["qpair_queue_request"].get<std::string>());
  }

  if (config["proposal_interval"] != NULL){
    *proposal_interval = stoi(config["proposal_interval"].get<std::string>());
  }

  if (config["read_amount_replica"] != NULL){
    *read_amount_replica = stoi(config["read_amount_replica"].get<std::string>());
  }

  if (config["number_of_slots_to_read"] != NULL){
    *number_of_slots_to_read = stoi(config["number_of_slots_to_read"].get<std::string>());
  }
}

int main(int argc, char *argv[]) {
  int N_PROCESSES = -1,N_LANES = -1,PID = -1;
  bool local = false;

  std::string cpumask = "";
  std::vector<std::string> trids;

  std::string s = "";
  std::string config_file = "";
  std::string app_name = "DiskPaxosEnv";

  int qpair_queue_size = 2048, qpair_queue_request = 2048, proposal_interval = 500;
  int read_amount_replica,number_of_slots_to_read;

  const struct option longopts[] =
  {
    {"processes",required_argument,0, 'n'},
    {"lanes",required_argument,0,'l'},
    {"pid",required_argument,0, 'i'},
    {"local",no_argument,0, 'b'},
    {"help",no_argument,0,'h'},
    {"cpumask",required_argument,0,'m'},
    {"nvmf",required_argument,0,'s'},
    {"config",required_argument,0,'c'},
    {0,0,0,0},
  };

  int option_index = 0;
  int c;
  while (1) {
    c = getopt_long(argc, argv, "n:l:bi:hs:c:",longopts, &option_index);
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
          read_amount_replica = N_LANES;
          number_of_slots_to_read = N_LANES;
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
      case 's':
          s = std::string(optarg);
          std::cout << "Trids: " << s << std::endl;
          break;
      case 'c':
          config_file = std::string(optarg);
          std::cout << "Config File: " << config_file << std::endl;
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

  if (s.size() > 0){
    compute_trids(s, &trids);
  }

  if (config_file.size() > 0 )
    setConfig(config_file,&app_name,&qpair_queue_size,&qpair_queue_request,&proposal_interval,&read_amount_replica,&number_of_slots_to_read);

  //std::vector<std::string> example {"trtype:TCP adrfam:IPv4 traddr:127.0.0.1 trsvcid:4420 subnqn:nqn.2016-06.io.spdk:cnode1"};
  SPDK_ENV::SPDK_ENV_OPTS env_opts(N_PROCESSES,N_LANES,cpumask,app_name,qpair_queue_size,qpair_queue_request);
  MultiReplicaPaxos::MultiReplicaPaxosOpts replica_opts(read_amount_replica,proposal_interval);
  LeaderPaxos::LeaderPaxosOpts leader_opts(N_LANES,number_of_slots_to_read);


  env_opts.print();
  replica_opts.print();
  leader_opts.print();


  if (local){
    const char * c_mask = cpumask.c_str();
    SPDK_ENV::spdk_start(N_PROCESSES,N_LANES,c_mask);
  }
  else{
    SPDK_ENV::spdk_start(env_opts,trids);
  }


  LeaderPaxos::LeaderPaxos lp(PID,leader_opts);
  std::thread leader_thread(&LeaderPaxos::LeaderPaxos::run,&lp);
  MultiReplicaPaxos::MultiReplicaPaxos rp(PID,replica_opts);
  rp.run();
  std::cout << "Exiting and starting spdk closure" << std::endl;
  leader_thread.join();


  //SglOpts::basic_write(0);

  SPDK_ENV::spdk_end();
  std::cout << "Process with pid: " << PID << " quiting" << std::endl;

  return 0;
}
