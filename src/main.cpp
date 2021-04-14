#include <iostream>
#include <array>
#include <iostream>
#include "BankCommand.hpp"
#include "DiskBlock.hpp"
#include <sstream>
#include <cereal/archives/binary.hpp>
#include "DiskAccess.hpp"
#include <chrono>
#include <thread>
#include <cstring>
#include <stdio.h>
#include <vector>



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
	int res = spdk_library_start();

	if (res)
		return -1;

	vector<int> aux = {105,79,85,92,83};
  BankCommand bs = BankCommand("Teste",aux);
	vector<int> aux2 = {45,123,2};
	BankCommand bs2 = BankCommand("Teste2",aux);
	string s2 = bs.serialize();
	string s = bs.serialize();
	DiskBlock db = DiskBlock();
  db.input = s;

	DiskBlock db2 = DiskBlock();
	db2.input = s2;
	db2.bal = 0;
	db2.mbal = 1;
	db2.slot = 4;

	/**
	string d_ser = db.serialize();
	char * db_serialized_char = &d_ser[0];
	string rec(db_serialized_char,d_ser.length());

	std::cout << d_ser.length() << std::endl;
	std::cout << db_serialized_char << ":1" << std::endl;
	std::cout << d_ser << std::endl;
	std::cout << rec << std::endl;

	byte * buffer = new byte[0x1000];
	string_to_bytes(d_ser, buffer);
	std::string result = bytes_to_string(buffer);
	std::cout << result << std::endl;

	bool t = result == d_ser;
	std::cout << t << std::endl;*/

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
	}

	spdk_library_end();

  return 0;
}
