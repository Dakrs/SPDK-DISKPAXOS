#ifndef DISKPAXOS_HPP
#define DISKPAXOS_HPP

#include <memory>
#include <vector>
#include <set>
#include <map>
#include "DiskBlock.hpp"
#include <chrono>
#include <atomic>
#include <future>

namespace DiskPaxos {
  typedef unsigned char byte;

  class DiskPaxos {
    public:
      int tick;
      int phase;
      int status; //0 okay, 1 completed, 2 cancel
      int slot;
      int pid;
      int nextBallot;
      int n_events;
      std::unique_ptr<DiskBlock> local_block;
      std::vector< std::unique_ptr<DiskBlock> > blocksSeen;
      std::set<std::string> disksSeen;
      std::string input;
      uint32_t target_core;
      std::atomic<int> finished; //0 running, 1 completed, 2 cleaning up;

      DiskPaxos(std::string input, int slot, int pid);
      void initPhase();
      void startBallot();
      void ReadAndWrite();
      void endPhase();
      void Cancel();
      void Abort(int mbal);
      void phase2();
      void Commit();
  };

  void launch_DiskPaxos(DiskPaxos * dp);
  void launch_DiskPaxos(DiskPaxos * dp,uint32_t target_core);
  void propose(int pid, int slot, std::string command);
  void propose(int pid, int slot, std::string command,uint32_t target_core);
  std::future<std::unique_ptr<std::map<int,DiskBlock>> > read_proposals(int k,int number_of_slots);
  std::future<std::unique_ptr<std::map<int,DiskBlock>> > read_proposals(int k,int number_of_slots,uint32_t target_core);
  std::future<DiskBlock> read_decision(int slot);
  std::future<DiskBlock> read_decision(int slot,uint32_t target_core);
}



#endif
