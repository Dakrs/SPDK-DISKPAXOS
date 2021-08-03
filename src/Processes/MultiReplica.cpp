#include "Processes/MultiReplica.hpp"
#include "Disk/DiskPaxos.hpp"
#include <chrono>
#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <thread>
#include <chrono>
#include <iostream>
#include <vector>
#include <future>
#include <map>
#include <memory>

namespace MultiReplicaPaxos {
  MultiReplicaPaxos::MultiReplicaPaxos(int pid,int n_lanes){
    this->pid = pid;
    this->slot = 0;
    this->l_core = -1;
    this->searching = false;
    this->received_decisions = 0;
    this->N_LANES = n_lanes;
  }

  MultiReplicaPaxos::MultiReplicaPaxos(int pid,int l_core,int n_lanes){
    this->pid = pid;
    this->slot = 0;
    this->l_core = l_core;
    this->searching = false;
    this->received_decisions = 0;
    this->N_LANES = n_lanes;
  }

  MultiReplicaPaxos::~MultiReplicaPaxos(){
  }

  void MultiReplicaPaxos::run(){
    using namespace std::chrono_literals;

    std::string filename = "example_files/input-" + std::to_string(this->pid);
    std::ifstream infile(filename);
    std::string line;
    //int n_lines = 0;

    std::vector<std::string> lines;
    try{
      while (std::getline(infile,line)) {
        int i = this->propose(line);
        if (i >= this->received_decisions)
          this->decisionsTosolve.insert(i);
        this->handle_possible_decisions();
        std::this_thread::sleep_for(0.5ms);
      }

      while(this->decisionsTosolve.size() > 0 && this->slot != this->received_decisions)
        this->handle_possible_decisions();

      this->cleanup();
      std::cout << "Replica quiting after n_props: " << this->slot << " decisons size: " << this->decisions.size() << " pid: " << this->pid << '\n';
      std::cout << "Logging results " << std::endl;
      this->output();
    }
    catch (std::exception& e){
      std::cerr << "Exception caught : " << e.what() << std::endl;
    }
  }

  void MultiReplicaPaxos::receive(){
    auto res = this->new_decisions.get();

    std::map<int,std::string>::iterator it;
    std::set<int>::iterator set_it;

    for (auto & [slot, blk] : (*res)){
      it = this->decisions.find(slot); //if decision was not yet seen
      if (it == this->decisions.end()){
        //std::cout << "slot: " << slot << " decision: " << blk.input << '\n';
        this->decisions.insert(std::pair<int,std::string>(blk.slot,blk.input));
      }

      set_it = this->decisionsTosolve.find(slot);
      if (set_it != this->decisionsTosolve.end()){
        this->decisionsTosolve.erase(set_it);
        this->proposals.erase(this->proposals.find(slot));
      }
    }

    /**
      update received_decisions
    */
    while(true){
      it = this->decisions.find(this->received_decisions);
      if (it != this->decisions.end()){
        this->received_decisions++;
      }
      else{
        break;
      }
    }
    //std::cout << "pid: " << this->pid << " slot: " << this->received_decisions << " size: " << this->decisionsTosolve.size() << " decisions_found: " << res->size() << std::endl;

    this->searching = false;
  }

  void MultiReplicaPaxos::handle_possible_decisions(){
    if (!this->searching){
      this->searching = true;
      this->new_decisions = DiskPaxos::read_multiple_decisions(this->received_decisions,this->N_LANES);
    }

    const auto f_current_state = this->new_decisions.wait_until(std::chrono::system_clock::time_point::min());

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

  void MultiReplicaPaxos::cleanup(){

    std::cout << "Starting Cleanup on PID: " << this->pid << std::endl;
    while(this->searching){
      const auto f_current_state = this->new_decisions.wait_until(std::chrono::system_clock::time_point::min());

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
    std::cout << "Ending Cleanup on PID: " << this->pid << std::endl;
  }

  int MultiReplicaPaxos::propose(std::string command){
    int return_slot = this->slot;
    //DiskPaxos::propose(this->pid,this->slot,command,this->pid); //voltar a propor para um novo slot
    if (this->l_core >= 0)
      DiskPaxos::propose(this->pid,this->slot,command,this->l_core); //voltar a propor para um novo slot
    else
      DiskPaxos::propose(this->pid,this->slot,command); //voltar a propor para um novo slot

    this->proposals.insert(std::pair<int,std::string>(this->slot,command));
    this->slot++;
    return return_slot;
  }

  void MultiReplicaPaxos::output(){
    std::string filename = "output/output-" + std::to_string(this->pid);
    std::ofstream out(filename);

    for (auto & [slot, dec] : this->decisions){
      std::string rline = std::to_string(slot) + " " + dec + "\n"; //export to file
      out << rline; //export to file
    }

    out.close();
  }
}
