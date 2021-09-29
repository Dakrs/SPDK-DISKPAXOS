#ifndef LEADERPAXOS_HPP
#define LEADERPAXOS_HPP

#include <map>
#include <vector>
#include <string>
#include <queue>
#include <memory>
#include <future>
#include "Disk/DiskPaxos.hpp"
#include <chrono>

namespace LeaderPaxos {

  struct Proposal {
    int slot;
    std::string command;

    Proposal(int s, std::string c): slot(s), command(c) {};
  };

  struct LeaderPaxosOpts {
    int number_of_lanes;
    int number_of_proposals_read;

    LeaderPaxosOpts(int lanes,int read_amount);
    LeaderPaxosOpts(int lanes);
    LeaderPaxosOpts();
    ~LeaderPaxosOpts();

    void print();
  };

  class LeaderPaxos {
    int pid; // process id
    int latest_slot; // lastest slot number added to proposals
    std::map<int,Proposal> proposals;
    std::vector<DiskPaxos::DiskPaxos *> slots; // currently running consensus
    std::map<int,DiskPaxos::DiskPaxos *> waiting_for_cleanup; // consensus finished waiting to be cleaned up
    std::vector<std::queue<Proposal>> queues; // queue for each lane
    bool searching; //boolean flag to keep track of searching for blocks
    std::future<std::unique_ptr<std::map<int,DiskBlock>> > props; //future with a map of proposals
    bool aborting;
    std::chrono::high_resolution_clock::time_point last_proposal_found;
    LeaderPaxosOpts opts;



    public:
      LeaderPaxos(int pid,int NUM_LANES);
      LeaderPaxos(int pid,LeaderPaxosOpts & leader_opts);
      void run();
      void update_slot();
      void cleanup();
      void search();
      void receive();
      void manage_consensus();
  };
}

#endif
