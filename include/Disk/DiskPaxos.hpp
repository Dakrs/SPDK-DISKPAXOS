#ifndef DISKPAXOS_HPP
#define DISKPAXOS_HPP

#include <memory>
#include <vector>
#include <set>
#include "DiskBlock.hpp"

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

    DiskPaxos(std::string input, int slot, uint32_t target_core, int pid);
    void initPhase();
    void startBallot();
    void ReadAndWrite();
    void endPhase();
    void Cancel();
    void Abort(int mbal);
    void phase2();
};

int spdk_start(int n_p,int n_k);
void spdk_end();
void start_DiskPaxos(DiskPaxos * dp);



#endif
