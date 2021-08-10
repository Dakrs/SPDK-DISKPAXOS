#include <iostream>
#include <sstream>
#include "Disk/DiskAccess.hpp"
#include <cstring>
#include <stdio.h>
#include "Test/disk_isomorphic_test.hpp"

#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>

#define no_argument 0
#define required_argument 1
#define optional_argument 2

void helpFunction();
bool verify_args(int N_PROCESSES, int N_LANES, int N_PROPOSALS, std::string diskid);

void helpFunction()
{
   std::cout << "Usage: sudo ./Reset --processes 8 --lanes 10 --proposals 40 -i 127.0.0.1" << std::endl;
   std::cout << "\t-n or --processes specifies the numbers of processes REQUIRED" << '\n';
   std::cout << "\t-l or --lanes specifies the numbers of lanes REQUIRED" << '\n';
   std::cout << "\t-p or --proposals specifies the numbers of proposals REQUIRED" << '\n';
   std::cout << "\t-i or --diskid specifies the disk identifier REQUIRED" << '\n';
   std::cout << "\t-b or --local indicates that a local device should be used" << '\n';
}

bool verify_args(int N_PROCESSES, int N_LANES, int N_PROPOSALS, std::string diskid){

  if (N_PROCESSES == -1){
    std::cout << "Error: Option -n or --processes must be specified" << std::endl;
    return false;
  }

  if (N_LANES == -1){
    std::cout << "Error: Option -l or --lanes must be specified" << std::endl;
    return false;
  }

  if (N_PROPOSALS == -1){
    std::cout << "Error: Option -p or --proposals must be specified" << std::endl;
    return false;
  }

  if (diskid == ""){
    std::cout << "Error: Option -i or --diskid must be specified" << std::endl;
    return false;
  }

  return true;
}

int main(int argc, char *argv[]) {
  int N_PROCESSES = -1,N_LANES = -1,N_PROPOSALS = -1;
  bool local = false;

  std::string disk_string = "";
  std::string subnqn = "";
  std::string port = "";

  const struct option longopts[] =
  {
    {"processes",required_argument,0, 'n'},
    {"lanes",required_argument,0,'l'},
    {"proposals",required_argument,0, 'p'},
    {"local",no_argument,0, 'b'},
    {"diskid",required_argument,0,'i'},
    {"help",no_argument,0,'h'},
    {"subnqn",required_argument,0,'s'},
    {"port",required_argument,0,'o'},
    {0,0,0,0},
  };

  int option_index = 0;
  int c;
  while (1) {
    c = getopt_long(argc, argv, "n:l:p:bi:h",longopts, &option_index);
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
      case 'p':
          N_PROPOSALS = atoi(optarg);
          std::cout << "N_PROPOSALS: " << N_PROPOSALS << std::endl;
          break;
      case 'b':
          local = true;
          std::cout << "Using local device" << std::endl;
          break;
      case 'i':
          disk_string = std::string(optarg);
          std::cout << "DiskString: " << disk_string << std::endl;
          break;
      case 's':
          subnqn = std::string(optarg);
          std::cout << "subnqn: " << subnqn << std::endl;
          break;
      case 'o':
          port = std::string(optarg);
          std::cout << "port: " << port << std::endl;
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

  if(!verify_args(N_PROCESSES,N_LANES,N_PROPOSALS,disk_string)){
      helpFunction();
      exit(-1);
  }

  int res;
  char * trid = new char[300];
  if (local){
    delete trid;
    trid = NULL;
  }
  else{
    snprintf(trid,300,"trtype:TCP adrfam:IPv4 traddr:127.0.0.1 trsvcid:%s subnqn:%s",port.c_str(),subnqn.c_str());
    //strcpy(trid,"trtype:TCP adrfam:IPv4 traddr:127.0.0.1 trsvcid:4421 subnqn:nqn.2016-06.io.spdk:cnode2");
  }

  res = spdk_library_start(N_PROCESSES,trid);

  if (res){
    std::cout << "Error initializing spdk" << '\n';
    exit(-1);
  }

  /**
  for(int i = 0; i < 4; i++){
    std::future<std::unique_ptr<DiskBlock>> f3 = read(disk_string,i);
    auto db2 = f3.get();

    std::cout << db2->toString() << std::endl;
  }*/

  int n_blocks = 512;
  int m_div;
  int remaining;
  int i;

  if(N_PROCESSES*N_LANES > 512){
    m_div = N_PROCESSES*N_LANES / n_blocks;
    remaining = N_PROCESSES*N_LANES % n_blocks;

    for (i = 0; i < m_div; i++) {
      auto fcons = initialize(disk_string, n_blocks , i*n_blocks); //proposals
      fcons.get();
      std::cout << "Consensus blocks reseted " << i *n_blocks << " to " << (i+1) * n_blocks << '\n';
    }

    if (remaining > 0){
      auto fcons_rem = initialize(disk_string, remaining , i*n_blocks); //proposals
      fcons_rem.get();
    }
  }
  else{
    auto f = initialize(disk_string, N_PROCESSES*N_LANES , 0); //reset blocks
    f.get();
    std::cout << "Consensus blocks reseted" << '\n';
  }

  if (N_PROPOSALS > 512){
    m_div = N_PROPOSALS / n_blocks;
    remaining = N_PROPOSALS % n_blocks;

    for (i = 0; i < m_div; i++) {
      auto fdec = initialize(disk_string, n_blocks , N_PROCESSES*N_LANES + i*n_blocks); //proposals
      fdec.get();
      std::cout << "Decision blocks reseted " << i *n_blocks << " to " << (i+1) * n_blocks << '\n';
    }

    if (remaining > 0){
      auto fdec_rem = initialize(disk_string, remaining , N_PROCESSES*N_LANES + i*n_blocks); //proposals
      fdec_rem.get();
    }
  }
  else{
    auto f1 = initialize(disk_string, N_PROPOSALS , N_PROCESSES*N_LANES); //decisions
    f1.get();
    std::cout << "Decision blocks reseted" << '\n';
  }

  if (N_PROCESSES*N_PROPOSALS > 512){
    n_blocks = 512;
    m_div = N_PROCESSES*N_PROPOSALS / n_blocks;
    remaining = N_PROCESSES*N_PROPOSALS % n_blocks;
    i = 0;
    for (i = 0; i < m_div; i++) {
      auto f2 = initialize(disk_string, n_blocks , 5000000 + N_PROCESSES*N_LANES + i*n_blocks); //proposals
      f2.get();
      std::cout << "RESET PROPOSTALS FROM BLOCKS " << i *n_blocks << " to " << (i+1) * n_blocks << '\n';
    }

    if (remaining > 0){
      auto f4 = initialize(disk_string, remaining , 5000000 + N_PROCESSES*N_LANES + i*n_blocks); //proposals
      f4.get();
    }
  }
  else{
    auto f3 = initialize(disk_string, N_PROCESSES*N_PROPOSALS , 5000000 + N_PROCESSES*N_LANES); //proposals
    f3.get();
  }
  std::cout << "Proposals blocks reseted" << '\n';

  //DiskTest disktest(20,4,argv[4],"trtype:TCP adrfam:IPv4 traddr:127.0.0.1 trsvcid:4420 subnqn:nqn.2016-06.io.spdk:cnode1"); //lanes, n_processes
  //disktest.run_every_test(1000);

  spdk_library_end();
  delete trid;

  return 0;
}
