#include <array>
#include <iostream>
#include "BankCommand.hpp"
#include "Disk/DiskBlock.hpp"
#include <sstream>
#include <cereal/archives/binary.hpp>
#include "Disk/DiskAccess.hpp"
#include <chrono>
#include <thread>
#include <cstring>
#include <stdio.h>
#include <vector>
#include "Test/disk_isomorphic_test.hpp"
#include "message.pb.h"
#include "Disk/DiskPaxos.hpp"

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

using namespace std;

int main(int argc, char const *argv[]) {

  /**
  spdk_start(8,10); // 8 processos, 10 lanes
  int pid = 7;

  DiskPaxos * dp = new DiskPaxos("test",1,0,pid);
  start_DiskPaxos(dp);

  std::this_thread::sleep_for (std::chrono::seconds(5));
  cout << dp->status << endl;


  spdk_end();*/
  /**
  int res = spdk_library_start(8);

  DiskBlock db2 = DiskBlock();
  db2.input = "";
  db2.bal = 0;
  db2.mbal = 20;
  db2.slot = 1;

  std::future<void> f2 = write("0000:03:00.0",db2,1,4);
  f2.get();

  spdk_library_end();*/


  spdk_start(8,10); // 8 processos, 10 lanes
  int pid = 7;

  DiskPaxos * dp = new DiskPaxos("test",1,0,pid);
  start_DiskPaxos(dp);

  std::this_thread::sleep_for (std::chrono::seconds(5));
  cout << dp->status << endl;


  spdk_end();

  return 0;
}
