#include <array>
#include <iostream>
#include "Disk/DiskBlock.hpp"
#include <sstream>
#include <cereal/archives/binary.hpp>
#include "Disk/DiskAccess.hpp"
#include <chrono>
#include <thread>
#include <cstring>
#include <stdio.h>
#include <vector>
#include "Test/disk_isomorphic_test.hpp"

int main(int argc, char const *argv[]) {
  if (argc < 5){
    std::cout << "Invalid number of args" << '\n';
    exit(-1);
  }

  int N_PROCESSES,N_LANES,N_PROPOSALS;

  N_PROCESSES = atoi(argv[1]);
  N_LANES = atoi(argv[2]);
  N_PROPOSALS = atoi(argv[3]);



  std::cout << "N_PROCESSES: " << N_PROCESSES << '\n';
  std::cout << "N_LANES: " << N_LANES << '\n';

  int res = spdk_library_start(N_PROCESSES,"trtype:TCP adrfam:IPv4 traddr:127.0.0.1 trsvcid:4420 subnqn:nqn.2016-06.io.spdk:cnode1");


  auto f = initialize(argv[4], N_PROCESSES*N_LANES , 0); //reset blocks
  f.get();
  auto f1 = initialize(argv[4], N_PROPOSALS , N_PROCESSES*N_LANES); //decisions
  f1.get();

  auto f2 = initialize(argv[4], N_PROCESSES*N_PROPOSALS , 5000000 + N_PROCESSES*N_LANES); //proposals
  f2.get();

  //DiskTest disktest(20,4,argv[4],"trtype:TCP adrfam:IPv4 traddr:127.0.0.1 trsvcid:4420 subnqn:nqn.2016-06.io.spdk:cnode1"); //lanes, n_processes
  //disktest.run_every_test(1000);

  spdk_library_end();

  return 0;
}
