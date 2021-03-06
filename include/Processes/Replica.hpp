#ifndef REPLICAPAXOS_HPP
#define REPLICAPAXOS_HPP

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


namespace ReplicaPaxos {
  class Decision {
    std::future<DiskBlock> res;
    int l_core;

    public:
      const int slot;
      Decision(int slot);
      Decision(int slot,int l_core);
      bool isReady();
      DiskBlock get();
  };

  class ReplicaPaxos {
    std::unordered_set<std::string> received_commands;
    std::map<int,std::string> decisions;
    std::map<int,std::string> proposals;
    std::set<std::unique_ptr<Decision>> decisionsTosolve;
    int slot;
    int pid;
    int l_core;

    public:
      ReplicaPaxos(int pid);
      ReplicaPaxos(int pid,int l_core);
      ~ReplicaPaxos();
      void run();
      void output();
      void handle_possible_decisions();
      int propose(std::string command);
  };
}

#endif
