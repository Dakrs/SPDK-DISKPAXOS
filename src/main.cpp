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



using namespace std;

int main(int argc, char const *argv[]) {
  /**
  std::cout << "Hello World!" << endl;

  vector<int> aux = {105,79,85,92,83};
  BankCommand bs = BankCommand("Teste",aux);

  cout << bs.toString() << endl;

  string s = bs.serialize();

  BankCommand bs1;
  bs1.deserialize(s);

  cout << bs1.toString() << endl;

  DiskBlock db = DiskBlock();
  db.input = s;
  string db_serialized = db.serialize();

  cout << db.toString() << endl;

  DiskBlock db2 = DiskBlock();
  db2.deserialize(db_serialized);

  cout << db2.toString() << endl;*/

	/*
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
	std::future<void> f1 = write("0000:03:00.0",db,0,0);
	f1.get();
	std::cout << "write completed" << std::endl;
	std::future<void> f2 = write("0000:03:00.0",db2,0,1);
	f2.get();
	std::cout << "write completed" << std::endl;
	std::future<unique_ptr<DiskBlock>> f3 = read("0000:03:00.0",0,0);
	auto db3 = f3.get();
	std::cout << "read completed: " << db3->toString() << std::endl;
	std::future<unique_ptr<DiskBlock>> f4 = read("0000:03:00.0",0,1);
	auto db4 = f4.get();
	std::cout << "read completed: " << db4->toString() << std::endl;
	std::future<unique_ptr<vector<unique_ptr<DiskBlock>>>> f5 = read_full("0000:03:00.0",0);
	auto db5 = f5.get();

	std::vector<unique_ptr<DiskBlock>>::iterator it = db5->begin();

	for(; it != db5->end(); it++){
		std::cout << (*it)->toString() << std::endl;
	}*/

	//spdk_library_end();

	/**

	#### AUTOMATED TESTING ####

	DiskTest disktest(20,4); //lanes, n_processes

	bool res = disktest.single_write_read_test();
	bool res2 = disktest.multi_write_read_test(100);

	std::cout << res << std::endl;
	std::cout << res2 << std::endl;
	*/

	/**
	int res = spdk_library_start(4);

	std::future<unique_ptr<DiskBlock>> f3 = read("0000:03:00.0",5,3);
	auto dbf = f3.get();
	std::cout << "read completed: " << dbf->toString() << std::endl;

	spdk_library_end();*/

  return 0;
}
