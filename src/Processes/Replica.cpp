#include "Processes/Replica.hpp"
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

namespace ReplicaPaxos {

  Decision::Decision(int slot): slot(slot){
    this->res = DiskPaxos::read_decision(slot);
  }

  Decision::Decision(int slot,int l_core): slot(slot){
    this->res = DiskPaxos::read_decision(slot,l_core);
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
    this->l_core = -1;
  }

  ReplicaPaxos::ReplicaPaxos(int pid,int l_core){
    this->pid = pid;
    this->slot = 0;
    this->l_core = l_core;
  }

  ReplicaPaxos::~ReplicaPaxos(){
  }

  void ReplicaPaxos::run(){
    using namespace std::chrono_literals;

    std::string filename = "example_files/input-" + std::to_string(this->pid);
    std::ifstream infile(filename);
    std::string line;
    Decision * d;
    //int n_lines = 0;

    std::vector<std::string> lines;
    try{
      while (std::getline(infile,line)) {
        int i = this->propose(line);

        if (this->l_core >= 0)
          d = new Decision(i,this->l_core);
        else
          d = new Decision(i);

        this->decisionsTosolve.insert(std::unique_ptr<Decision>(d));
        this->handle_possible_decisions();
        std::this_thread::sleep_for(0.5ms);
      }

      while(this->decisionsTosolve.size() > 0)
        this->handle_possible_decisions();
      std::cout << "Replica quiting after n_props: " << this->slot << " decisons size: " << this->decisions.size() << '\n';
      std::cout << "Logging results " << std::endl;
      this->output();

      /**
      int k = 0;
      int amount = 5;
      for(k = 0; k < 100; k+= 5){
        std::future<std::unique_ptr<std::map<int,DiskBlock>>> ft = DiskPaxos::read_multiple_decisions(k,amount);
        auto res = ft.get();

        for (auto & [slot, db] : (*res)){
          std::cout << "slot: " << db.slot << " input: " << db.input << '\n';
        }
      }*/
    }
    catch (std::exception& e){
      std::cerr << "Exception caught : " << e.what() << std::endl;
    }
  }

  void ReplicaPaxos::handle_possible_decisions(){
    std::vector<int> toRestart;
    std::map<int,std::string>::iterator it_map;

    for(auto it = this->decisionsTosolve.begin(); it != this->decisionsTosolve.end();){

      if ((*it)->isReady()){//a decision already finished
        auto db = (*it)->get(); //diskblock with result
        it_map = this->proposals.find(db.slot);

        this->decisions.insert(std::pair<int,std::string>(db.slot,db.input));

        this->proposals.erase(it_map->first);
        this->decisionsTosolve.erase(it++);
      }
      else{
        ++it;
      }
    }

    /**
    for(auto i : toRestart){
      Decision * d = new Decision(i);
      this->decisionsTosolve.insert(std::unique_ptr<Decision>(d));
    }*/
  }

  int ReplicaPaxos::propose(std::string command){
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

  void ReplicaPaxos::output(){
    std::string filename = "output/output-" + std::to_string(this->pid);
    std::ofstream out(filename);

    for (auto & [slot, dec] : this->decisions){
      std::string rline = std::to_string(slot) + " " + dec + "\n"; //export to file
      out << rline; //export to file
    }

    out.close();
  }
}
