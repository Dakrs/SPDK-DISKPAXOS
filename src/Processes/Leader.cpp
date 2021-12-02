extern "C" {
	#include "spdk/env.h"
}

#include "Processes/Leader.hpp"
#include "Disk/DiskPaxos.hpp"
#include <chrono>
#include <iostream>

namespace LeaderPaxos {

  LeaderPaxosOpts::LeaderPaxosOpts(int lanes,int read_amount,bool flag){
    this->number_of_lanes = lanes;
    this->number_of_proposals_read = read_amount;
    this->strip = flag;
  }

  LeaderPaxosOpts::LeaderPaxosOpts(int lanes){
    this->number_of_lanes = lanes;
    this->number_of_proposals_read = lanes;
    this->strip = false;
  }

  LeaderPaxosOpts::LeaderPaxosOpts(){
    this->number_of_lanes = 32;
    this->number_of_proposals_read = 32;
    this->strip = false;
  }

  LeaderPaxosOpts::~LeaderPaxosOpts(){}

  void LeaderPaxosOpts::print(){
    std::cout << "LeaderPaxosOpts Configs" << '\n';
    std::cout << "Amount Read in each Proposal: " << this->number_of_proposals_read << std::endl;
    std::cout << "Number of Lanes: " << this->number_of_lanes << std::endl;
  }

  Analyser::Analyser(int lanes){
    this->lanes = new uint64_t[lanes];
    this->n_concensus = 0;
    this->total_ticks = 0;
  }

  Analyser::Analyser(){
    this->lanes = NULL;
    this->n_concensus = 0;
    this->total_ticks = 0;
  }

  Analyser::~Analyser(){
    delete this->lanes;
  }

  void Analyser::start(int i){
    this->lanes[i] = spdk_get_ticks();
  }

  void Analyser::end(int i){
    this->total_ticks += (spdk_get_ticks() - this->lanes[i]);
    this->n_concensus++;
  }

  void Analyser::output(){
    double time_sec = ((double)this->total_ticks) / ((double) spdk_get_ticks_hz());

    double avg = (time_sec * 1000.0) / ((double) this->n_concensus);

    std::cout << this->n_concensus << " instances on an avg of " << avg << " ms" << std::endl;
  }



  //spdk_get_ticks()
  LeaderPaxos::LeaderPaxos(int pid,int NUM_LANES){
    this->opts = LeaderPaxosOpts(NUM_LANES);
    this->latest_slot = 0;
    this->pid = pid;
    this->slots = std::vector<DiskPaxos::DiskPaxos *>(NUM_LANES,NULL);
    this->queues = std::vector<std::queue<Proposal>>(NUM_LANES, std::queue<Proposal>());
    this->queues_sizes = std::vector<int>(NUM_LANES);
    this->searching = false;
    this->aborting = false;
    this->last_proposal_found = std::chrono::high_resolution_clock::now();
    this->stats = Analyser(NUM_LANES);
  }

  LeaderPaxos::LeaderPaxos(int pid,LeaderPaxosOpts & leader_opts){
    this->opts = std::move(leader_opts);
    this->latest_slot = 0;
    this->pid = pid;
    this->slots = std::vector<DiskPaxos::DiskPaxos *>(this->opts.number_of_lanes,NULL);
    this->queues = std::vector<std::queue<Proposal>>(this->opts.number_of_lanes, std::queue<Proposal>());
    this->queues_sizes = std::vector<int>(this->opts.number_of_lanes);
    this->searching = false;
    this->aborting = false;
    this->last_proposal_found = std::chrono::high_resolution_clock::now();
    this->stats = Analyser(this->opts.number_of_lanes);
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

      if (this->opts.strip)
        this->props = DiskPaxos::read_proposals_strip(this->latest_slot,this->opts.number_of_proposals_read);
      else
        this->props = DiskPaxos::read_proposals(this->latest_slot,this->opts.number_of_proposals_read);
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

      if (ms_int.count() >= 3){
        this->aborting = true;
        std::cout << "3 seconds without proposals, exiting" << std::endl;
        return;
      }
    }

    for (auto & [slot, blk] : (*res)){
      it = this->proposals.find(slot);
      if (it == this->proposals.end()){
        //std::cout << "Proposal for slot: " << blk.slot << " input: " << blk.input << " on PID: " << this->pid << std::endl;
        this->proposals.insert(std::pair<int,Proposal>(slot,Proposal(blk.slot,blk.input)));

        int target_slot = slot % this->opts.number_of_lanes;
        this->queues[target_slot].push(Proposal(blk.slot,blk.input));
        this->queues_sizes[target_slot]++;
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

  void LeaderPaxos::start_consensus(int i){
    Proposal p = this->queues[i].front();
    this->queues[i].pop();
    this->queues_sizes[i]--;

    //std::cout << "Launching slot: " << p.slot << '\n';
    DiskPaxos::DiskPaxos * dp = new DiskPaxos::DiskPaxos(p.command,p.slot,this->pid);
    this->slots[i] = dp;
    //DiskPaxos::launch_DiskPaxos(dp,this->pid); // spawns a new instance of the consensus protocol
    DiskPaxos::launch_DiskPaxos(dp);
  }

  void LeaderPaxos::manage_consensus(){
    for (int i = 0; i < this->opts.number_of_lanes; i++) {
      DiskPaxos::DiskPaxos * dp = this->slots[i];
      //se estiver livre ou se já tiver terminado
      if (dp == NULL && this->queues_sizes[i] > 0) {
        this->stats.start(i);
        this->start_consensus(i);
        continue;
      }

      if (dp != NULL && dp->finished > 0){
        if (dp->status == 2){
          this->aborting = true;
          break;
        }
        this->waiting_for_cleanup.insert(std::pair<int,DiskPaxos::DiskPaxos *>(dp->slot,dp));
        this->stats.end(i);
        if (this->queues_sizes[i] > 0){
          this->stats.start(i);
          this->start_consensus(i);
        }
        else {
          this->slots[i] = NULL;
        }
      }
    }
  }

  void LeaderPaxos::run(){
    std::map<int,Proposal>::iterator it;

    while(true){
      this->search(); //search for incoming proposals;

      this->manage_consensus();

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

    this->stats.output();

    std::cout << "Cleanup completed" << std::endl;
  }
}
