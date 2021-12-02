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

  struct Proposal {
    std::string command;
    int slot;
    std::chrono::high_resolution_clock::time_point start;
    std::chrono::high_resolution_clock::time_point end;

    Proposal(int slot,std::string command);
    void finish();
    ~Proposal();
  };

  struct MultiReplicaPaxosOpts {
    int decisions_read_amount;
    int proposal_interval;
    int propose_strip;
    bool benchmarking;

    MultiReplicaPaxosOpts(int dec,int interval,bool bench,int propose_strip);
    MultiReplicaPaxosOpts(int dec,int interval);
    MultiReplicaPaxosOpts(int dec);
    MultiReplicaPaxosOpts();
    ~MultiReplicaPaxosOpts();

    void print();
  };


  class MultiReplicaPaxos {
    std::unordered_set<std::string> received_commands;
    std::map<int,std::string> decisions;
    std::map<int,Proposal> proposals;
    std::set<int> decisionsTosolve;
    std::future<std::unique_ptr<std::map<int,DiskBlock>> > new_decisions; //future with a map of incoming decisions
    bool searching;
    int slot;
    int pid;
    int l_core;
    int received_decisions;
    MultiReplicaPaxosOpts opts;

    public:
      MultiReplicaPaxos(int pid, int n_lanes);
      MultiReplicaPaxos(int pid,int l_core, int n_lanes);
      MultiReplicaPaxos(int pid,MultiReplicaPaxosOpts & opts_tmp);
      ~MultiReplicaPaxos();
      void run();
      void output();
      void handle_possible_decisions();
      void receive();
      int propose(std::string command);
      void propose(std::vector<std::string>& commands);
      void cleanup();
      void latency();
  };
}

#endif
