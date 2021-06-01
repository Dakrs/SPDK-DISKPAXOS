#include "Processes/Replica.hpp"
#include "Disk/DiskPaxos.hpp"
#include <chrono>
#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <thread>
#include <chrono>
#include <fstream>
#include <iostream>

namespace ReplicaPaxos {

  Decision::Decision(int slot): slot(slot){
    this->res = DiskPaxos::read_decision(slot);
  }

  bool Decision::isReady(){
    const auto f_current_state = this->res.wait_until(std::chrono::system_clock::time_point::min());
    return f_current_state == std::future_status::ready;
  }

  DiskBlock Decision::get(){
    return this->res.get();
  }

  ReplicaPaxos::ReplicaPaxos(int pid){
    this->pid = pid;
    this->slot = 0;
    std::string filename = "output/output-" + std::to_string(this->pid);
    this->out = std::ofstream(filename);
  }

  ReplicaPaxos::~ReplicaPaxos(){
    this->out.close();
  }

  void ReplicaPaxos::run(){
    using namespace std::chrono_literals;

    std::string filename = "example_files/input-" + std::to_string(this->pid);
    std::ifstream infile(filename);
    std::string line;

    while (std::getline(infile,line)) {
      int i = this->propose(line);
      Decision * d = new Decision(i);
      this->decisionsTosolve.insert(std::unique_ptr<Decision>(d));
      this->handle_possible_decisions();
      std::this_thread::sleep_for(100ms);
    }

    while(this->decisionsTosolve.size() > 0)
      this->handle_possible_decisions();

    //std::this_thread::sleep_for(2000ms);
    //this->output();
  }

  void ReplicaPaxos::handle_possible_decisions(){
    std::vector<int> toRestart;
    std::map<int,std::string>::iterator it_map;

    for(auto it = this->decisionsTosolve.begin(); it != this->decisionsTosolve.end();){

      if ((*it)->isReady()){
        auto db = (*it)->get(); //diskblock with result
        it_map = this->proposals.find(db.slot);

        this->decisions.insert(std::pair<int,std::string>(db.slot,db.input));

        std::string rline = std::to_string(db.slot) + " " + db.input + "\n"; //export to file
        this->out << rline; //export to file
        //se a proposal existe e se a decisão é diferente da proposta

        /**
        if (it_map->second != db.input){
          //propor para um novo slot
          int n_slot = this->propose(it_map->second);
          toRestart.push_back(n_slot);
        }*/

        this->proposals.erase(it_map->first);
        this->decisionsTosolve.erase(it++);
      }
      else{
        ++it;
      }
    }

    for(auto i : toRestart){
      Decision * d = new Decision(i);
      this->decisionsTosolve.insert(std::unique_ptr<Decision>(d));
    }
  }

  int ReplicaPaxos::propose(std::string command){
    int return_slot = this->slot;
    //DiskPaxos::propose(this->pid,this->slot,command,this->pid); //voltar a propor para um novo slot
    DiskPaxos::propose(this->pid,this->slot,command); //voltar a propor para um novo slot

    this->proposals.insert(std::pair<int,std::string>(this->slot,command));
    this->slot++;
    return return_slot;
  }

  void ReplicaPaxos::output(){
    for (auto & [slot, dec] : this->decisions){
      std::cout << "KEY: " << slot << " " << "Decision: " << dec << std::endl;
    }
  }
}
