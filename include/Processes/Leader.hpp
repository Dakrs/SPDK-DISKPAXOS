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
    bool strip;

    LeaderPaxosOpts(int lanes,int read_amount,bool flag);
    LeaderPaxosOpts(int lanes);
    LeaderPaxosOpts();
    ~LeaderPaxosOpts();

    void print();
  };

  struct Analyser {
    int n_concensus;
    uint64_t * lanes;
    uint64_t total_ticks;

    Analyser(int lanes);
    Analyser();
    ~Analyser();
    void start(int i);
    void end(int i);
    void output();
  };

  class LeaderPaxos {
    public:
      int pid; // process id
      int latest_slot; // lastest slot number added to proposals
      std::map<int,Proposal> proposals;
      std::vector<DiskPaxos::DiskPaxos *> slots; // currently running consensus
      std::map<int,DiskPaxos::DiskPaxos *> waiting_for_cleanup; // consensus finished waiting to be cleaned up
      std::vector<std::queue<Proposal>> queues; // queue for each lane
      std::vector<int> queues_sizes;
      bool searching; //boolean flag to keep track of searching for blocks
      std::future<std::unique_ptr<std::map<int,DiskBlock>> > props; //future with a map of proposals
      bool aborting;
      std::chrono::high_resolution_clock::time_point last_proposal_found;
      LeaderPaxosOpts opts;
      Analyser stats;


      LeaderPaxos(int pid,int NUM_LANES);
      LeaderPaxos(int pid,LeaderPaxosOpts & leader_opts);
      void run();
      void update_slot();
      void cleanup();
      void search();
      void receive();
      void manage_consensus();
      void start_consensus(int i);
  };

  class LeaderPaxosBench: public LeaderPaxos {

    int nslot;
    public:
      LeaderPaxosBench(int pid,LeaderPaxosOpts & leader_opts);
      void run();
      void prepare();
  };
}

#endif
