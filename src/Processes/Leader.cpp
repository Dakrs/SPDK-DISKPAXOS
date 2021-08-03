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
    this->aborting = false;
    this->last_proposal_found = std::chrono::high_resolution_clock::now();
  }

  /**
    Method to update the latest_slot found;
  */

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

  /**
    Method for searching for new proposals
  */

  void LeaderPaxos::search(){
    if (!this->searching){
      this->searching = true;
      this->props = DiskPaxos::read_proposals(this->latest_slot,this->NUM_LANES);
    }

    const auto f_current_state = this->props.wait_until(std::chrono::system_clock::time_point::min());

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

  /**
    Method to handle the completion of reading proposals
  */

  void LeaderPaxos::receive(){
    std::map<int,Proposal>::iterator it;

    auto res = this->props.get();

    // para terminar os lideres, após 5s sem propostas os líderes acabam de correr.
    if (res->size() > 0){
      this->last_proposal_found = std::chrono::high_resolution_clock::now();
    }
    else{
      auto t2 = std::chrono::high_resolution_clock::now();
      auto ms_int = std::chrono::duration_cast<std::chrono::seconds>(t2 - this->last_proposal_found);

      if (ms_int.count() >= 5){
        this->aborting = true;
        std::cout << "5 secs without proposals, exiting" << std::endl;
        return;
      }
    }

    for (auto & [slot, blk] : (*res)){
      it = this->proposals.find(slot);
      if (it == this->proposals.end()){
        //std::cout << "Proposal for slot: " << blk.slot << " input: " << blk.input << " on PID: " << this->pid << std::endl;
        this->proposals.insert(std::pair<int,Proposal>(slot,Proposal(blk.slot,blk.input)));

        int target_slot = slot % this->NUM_LANES;
        this->queues[target_slot].push(Proposal(blk.slot,blk.input));
      }
    }
    this->searching = false;
  }

  /**
    Method to cleanup past DiskPaxos instances
  */

  void LeaderPaxos::cleanup(){
    for (auto it = this->waiting_for_cleanup.cbegin(); it != this->waiting_for_cleanup.cend();){
      if (it->second->finished == 1){
        //std::cout << "Deleting DiskPaxos for slot: " << it->second->slot << std::endl;
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

    while(true){
      this->search(); //search for incoming proposals;


      for (int i = 0; i < this->NUM_LANES; i++) {
        DiskPaxos::DiskPaxos * dp = this->slots[i];
        //se estiver livre ou se já tiver terminado
        if ((dp == NULL && this->queues[i].size() > 0) || (dp != NULL && dp->finished > 0 && this->queues[i].size() > 0)) {
          Proposal p = this->queues[i].front();
          this->queues[i].pop();
          if (dp != NULL){
            //check if a transaction was aborted
            if (dp->status == 2){
              this->aborting = true;
              break;
            }
            this->waiting_for_cleanup.insert(std::pair<int,DiskPaxos::DiskPaxos *>(dp->slot,dp));
          }
          dp = new DiskPaxos::DiskPaxos(p.command,p.slot,this->pid);
          this->slots[i] = dp;
          //DiskPaxos::launch_DiskPaxos(dp,this->pid); // spawns a new instance of the consensus protocol
          DiskPaxos::launch_DiskPaxos(dp);
        }
      }

      this->cleanup(); //clean up old allocated memory for consensus

      if (this->aborting){
        break;
      }
      this->update_slot();
    }
    std::cout << "Aborting - PID: " << this->pid << std::endl;

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
