#include <array>
#include <iostream>
#include "Disk/DiskBlock.hpp"
#include <sstream>
#include "Disk/DiskAccess.hpp"
#include <chrono>
#include <thread>
#include <cstring>
#include <stdio.h>
#include <vector>
#include "Test/disk_isomorphic_test.hpp"
#include "Disk/DiskPaxos.hpp"
#include "Disk/SPDK_ENV.hpp"
#include "Processes/Leader.hpp"
#include "Processes/Replica.hpp"
#include <exception>


/**
#### MULTIPLE REQUESTS ####

int res = spdk_library_start();

if (res)
	return -1;

vector<int> aux = {105,79,85,92,83};
BankCommand bs = BankCommand("Teste",aux);
vector<int> aux2 = {45,123,2};
BankCommand bs2 = BankCommand("Teste2",aux);
string s = bs.serialize();
string s2 = bs2.serialize();
DiskBlock db = DiskBlock();
db.input = s;

DiskBlock db2 = DiskBlock();
db2.input = s2;
db2.bal = 0;
db2.mbal = 1;
db2.slot = 4;

DiskBlock empty_block = DiskBlock();

int k = 20;
int NUM_PROCESSES = 4;

std::future<void> f1 = initialize("0000:03:00.0",k);
f1.get();
std::cout << "initialize block completed" << std::endl;

std::future<void> f2 = write("0000:03:00.0",db2,5,3);
std::future<void> f3 = write("0000:03:00.0",db,5,3);
std::future<void> f4 = write("0000:03:00.0",db2,5,2);

f2.get();
f3.get();
f4.get();


std::future<unique_ptr<vector<unique_ptr<DiskBlock>>>> f5 = read_full("0000:03:00.0",5);

auto db5 = f5.get();
std::vector<unique_ptr<DiskBlock>>::iterator it = db5->begin();

for(; it != db5->end(); it++){
	std::cout << (*it)->toString() << std::endl;
}*/


/**

#### AUTOMATED TESTING ####

DiskTest disktest(20,4); //lanes, n_processes

disktest.run_every_test(1000);

*/

/**
Message::Message sampleM;
sampleM.set_messagetype(Message::Message::REQUEST);
Message::RequestMessage * example1 = sampleM.mutable_req();
example1->set_userid(2);
example1->set_requestid(2);
example1->set_query("Protobdasdasd");

Message::RequestMessage example2 = sampleM.req();

std::cout << "userId: " << example2.userid() << " requestId: " << example2.requestid() << " Query: " << example2.query() << std::endl;

std::string s;
sampleM.SerializeToString(&s);

std::cout << "str: " << s << std::endl;

Message::Message sampleB1;
sampleB1.ParseFromString(s);

Message::RequestMessage sampleB = sampleB1.req();

std::cout << "userId: " << sampleB.userid() << " requestId: " << sampleB.requestid() << " Query: " << sampleB.query() << std::endl;

*/

/**
####### TESTING ########
DiskTest disktest(20,4); //lanes, n_processes
disktest.run_every_test(1000);
disktest.single_write_read_test(40,3);
*/

/**
int res = spdk_library_start(N_PROCESSES);
DiskBlock db2 = DiskBlock();
db2.input = "";
db2.bal = 0;
db2.mbal = 20;
db2.slot = 1;

std::future<void> f2 = write("0000:03:00.0",db2,1,4);
f2.get();

auto f = initialize("0000:03:00.0", N_PROCESSES*N_LANES , 0);
f.get();

spdk_library_end();*/


//spdk_start(N_PROCESSES,N_LANES); // 8 processos, 10 lanes
//int pid = 7;

/**
propose(pid,0,"opttest");
propose(5,0,"opttest_fst");
propose(pid,1,"opttest_snd");
std::this_thread::sleep_for (std::chrono::seconds(2));

std::future<std::unique_ptr<std::map<int,DiskBlock>> > f = read_proposals(0,2);
auto res = f.get();

for (auto & [key, val] : (*res))
{
  std::cout << "KEY: " << key << " val: " << val.toString()  << '\n';
}*/

//spdk_end();
/**
DiskPaxos::DiskPaxos * dp0 = new DiskPaxos::DiskPaxos("test0",0,pid);
start_DiskPaxos(dp0);

std::this_thread::sleep_for (std::chrono::seconds(1));

DiskPaxos::DiskPaxos * dp1 = new DiskPaxos::DiskPaxos("test1",1,pid);
start_DiskPaxos(dp1);

std::this_thread::sleep_for (std::chrono::seconds(1));

DiskPaxos::DiskPaxos * dp2 = new DiskPaxos::DiskPaxos("test2",2,pid);
start_DiskPaxos(dp2);

std::this_thread::sleep_for (std::chrono::seconds(1));

DiskPaxos::DiskPaxos * dp3 = new DiskPaxos::DiskPaxos("test3",3,pid);
start_DiskPaxos(dp3);

std::this_thread::sleep_for (std::chrono::seconds(1));

DiskPaxos::DiskPaxos * dp4 = new DiskPaxos::DiskPaxos("test4",4,pid);
start_DiskPaxos(dp4);

std::this_thread::sleep_for (std::chrono::seconds(1));

DiskPaxos::DiskPaxos * dp5 = new DiskPaxos::DiskPaxos("test5",5,pid);
start_DiskPaxos(dp5);

std::this_thread::sleep_for (std::chrono::seconds(10));
cout << "consensus id: 0 status: " << dp0->status << " finished: " << dp0->finished << endl;
cout << "consensus id: 1 status: " << dp1->status << " finished: " << dp1->finished << endl;
cout << "consensus id: 2 status: " << dp2->status << " finished: " << dp2->finished << endl;
cout << "consensus id: 3 status: " << dp3->status << " finished: " << dp3->finished << endl;
cout << "consensus id: 4 status: " << dp4->status << " finished: " << dp4->finished << endl;
cout << "consensus id: 5 status: " << dp5->status << " finished: " << dp5->finished << endl;*/

/**
DiskPaxos::propose(pid,0,"opttest");
DiskPaxos::propose(5,0,"opttest_fst");
DiskPaxos::propose(pid,1,"opttest_snd");
std::this_thread::sleep_for (std::chrono::seconds(2));

std::future<std::unique_ptr<std::map<int,DiskBlock>> > f = DiskPaxos::read_proposals(0,2);
auto res = f.get();

for (auto & [key, val] : (*res))
{
  std::cout << "KEY: " << key << " val: " << val.toString()  << '\n';
}*/


using namespace std;

std::thread spawn_process(int pid, int N_LANES);

std::thread spawn_process(int pid, int N_LANES){
  std::thread t([pid,N_LANES](){

    LeaderPaxos::LeaderPaxos lp(pid,N_LANES);
    std::thread leader_thread(&LeaderPaxos::LeaderPaxos::run,&lp);
    ReplicaPaxos::ReplicaPaxos rp(pid,0);
    rp.run();
    leader_thread.join();
  });

  return t;
}

int main(int argc, char const *argv[]) {
  int N_PROCESSES = 8, N_LANES = 10;
  if (argc < 5 || atoi(argv[1]) >= N_PROCESSES || atoi(argv[2]) >= N_PROCESSES){
    std::cout << "Error on args used" << std::endl;
    return -1;
  }

  //std::vector<std::string> example {"trtype:TCP adrfam:IPv4 traddr:127.0.0.1 trsvcid:4420 subnqn:nqn.2016-06.io.spdk:cnode1"};
  std::string cpumask = std::string(argv[4]);

  const char * c_mask = cpumask.c_str();

  std::cout << c_mask << std::endl;

  SPDK_ENV::spdk_start(N_PROCESSES,N_LANES,c_mask);

  SPDK_ENV::print_addresses();

  int pid_1 = atoi(argv[1]);
  int pid_2 = atoi(argv[2]);
  int pid_3 = atoi(argv[3]);


  /**
  LeaderPaxos::LeaderPaxos lp(pid,N_LANES);
  std::thread t(&LeaderPaxos::LeaderPaxos::run,&lp);
  ReplicaPaxos::ReplicaPaxos rp(pid);
  rp.run();
  t.join();*/


  std::thread t = spawn_process(pid_1,N_LANES);
  std::thread t2 = spawn_process(pid_2,N_LANES);
  std::thread t3 = spawn_process(pid_3,N_LANES);

  t.join();
  t2.join();
  t3.join();
  //std::terminate(t);


  SPDK_ENV::spdk_end();

  return 0;
}
