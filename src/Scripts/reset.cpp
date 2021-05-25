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

int main(int argc, char const *argv[]) {
  if (argc < 3){
    std::cout << "Invalid number of args" << '\n';
    exit(-1);
  }

  int N_PROCESSES,N_LANES;

  N_PROCESSES = atoi(argv[1]);
  N_LANES = atoi(argv[2]);


  std::cout << "N_PROCESSES: " << N_PROCESSES << '\n';
  std::cout << "N_LANES: " << N_LANES << '\n';

  int res = spdk_library_start(N_PROCESSES);
  auto f = initialize("0000:03:00.0", N_PROCESSES*N_LANES , 0);
  f.get();
  spdk_library_end();

  return 0;
}
