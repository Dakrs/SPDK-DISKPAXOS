#include "Processes/Leader.hpp"
#include "Disk/DiskPaxos.hpp"
#include <chrono>
#include <iostream>

namespace LeaderPaxos {
  LeaderPaxos::LeaderPaxos(int pid,int NUM_LANES){
    this->NUM_LANES = NUM_LANES;
    this->latest_slot = 0;
    this->pid = pid;
    this->slots = std::vector<DiskPaxos::DiskPaxos *>(NUM_LANES,NULL);
    this->queues = std::vector<std::queue<Proposal>>(NUM_LANES, std::queue<Proposal>());
    this->searching = false;
  }

  void LeaderPaxos::update_slot(){
    std::map<int,Proposal>::iterator it;
    while(true){
      it = this->proposals.find(this->latest_slot);
      if (it != this->proposals.end()){
        this->latest_slot++;
      }
      else{
        break;
      }
    }
  }

  void LeaderPaxos::search(){
    if (!this->searching){
      this->searching = true;
      this->props = DiskPaxos::read_proposals(this->latest_slot,2);
    }

    const auto f_current_state = this->props.wait_for(std::chrono::seconds(0));

    switch (f_current_state) {
      case std::future_status::deferred:
        break;
      case std::future_status::ready:
        this->receive();
        break;
      case (std::future_status::timeout):
        break;
      default:
        break;
    }
  }

  void LeaderPaxos::receive(){
    std::map<int,Proposal>::iterator it;

    auto res = this->props.get();
    for (auto & [slot, blk] : (*res)){
      it = this->proposals.find(slot);
      if (it == this->proposals.end()){
        this->proposals.insert(std::pair<int,Proposal>(slot,Proposal(blk.slot,blk.input)));

        int target_slot = slot % this->NUM_LANES;
        this->queues[target_slot].push(Proposal(blk.slot,blk.input));
      }
    }
    this->searching = false;
  }

  void LeaderPaxos::cleanup(){
    for (auto it = this->waiting_for_cleanup.cbegin(); it != this->waiting_for_cleanup.cend();){
      if (it->second->finished == 1){
        std::cout << "Deleting DiskPaxos for slot: " << it->second->slot << std::endl;
        delete it->second;
        this->waiting_for_cleanup.erase(it++);
      }
      else{
        ++it;
      }
    }
  }

  void LeaderPaxos::run(){
    std::map<int,Proposal>::iterator it;
    bool abort = false;

    while(true){
      this->search();

      for (int i = 0; i < this->NUM_LANES; i++) {
        DiskPaxos::DiskPaxos * dp = this->slots[i];
        //se estiver livre ou se já tiver terminado
        if ((dp == NULL && this->queues[i].size() > 0) || (dp != NULL && dp->finished > 0 && this->queues[i].size() > 0)) {
          Proposal p = this->queues[i].front();
          this->queues[i].pop();
          if (dp != NULL){
            //check if a transaction was aborted
            if (dp->status == 2){
              abort = true;
              break;
            }
            this->waiting_for_cleanup.insert(std::pair<int,DiskPaxos::DiskPaxos *>(dp->slot,dp));
          }
          dp = new DiskPaxos::DiskPaxos(p.command,p.slot,this->pid);
          this->slots[i] = dp;
          DiskPaxos::launch_DiskPaxos(dp);
        }
      }

      if (abort){
        break;
      }
      this->update_slot();

      if (this->latest_slot > 9)
        break;
    }

    for(DiskPaxos::DiskPaxos * dp : slots){
      if (dp != NULL){
        this->waiting_for_cleanup.insert(std::pair<int,DiskPaxos::DiskPaxos *>(dp->slot,dp));
      }
    }

    while(this->waiting_for_cleanup.size() > 0)
      this->cleanup();

    std::cout << "Cleanup completed" << std::endl;
  }
}
