#ifndef MULTIREPLICAPAXOS_HPP
#define MULTIREPLICAPAXOS_HPP

#include <map>
#include <unordered_set>
#include <vector>
#include <string>
#include <memory>
#include <future>
#include "Disk/DiskPaxos.hpp"
#include "Disk/DiskBlock.hpp"
#include <fstream>
#include <iostream>


namespace MultiReplicaPaxos {
  class MultiReplicaPaxos {
    std::unordered_set<std::string> received_commands;
    std::map<int,std::string> decisions;
    std::map<int,std::string> proposals;
    std::set<int> decisionsTosolve;
    std::future<std::unique_ptr<std::map<int,DiskBlock>> > new_decisions; //future with a map of incoming decisions
    bool searching;
    int slot;
    int pid;
    int l_core;
    int received_decisions;
    int N_LANES;

    public:
      MultiReplicaPaxos(int pid, int n_lanes);
      MultiReplicaPaxos(int pid,int l_core, int n_lanes);
      ~MultiReplicaPaxos();
      void run();
      void output();
      void handle_possible_decisions();
      void receive();
      int propose(std::string command);
      void cleanup();
  };
}

#endif
