#include "Disk/DiskBlock.hpp"
#include "Disk/DiskAccess.hpp"
#include "Test/disk_isomorphic_test.hpp"
#include <iostream>
#include <ctime>
#include <unistd.h>
#include <random>
#include <stdlib.h>
#include <string>

static std::string gen_random(const int len) {

    std::string tmp_s;
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

    tmp_s.reserve(len);

    for (int i = 0; i < len; ++i)
        tmp_s += alphanum[rand() % (sizeof(alphanum) - 1)];


    return tmp_s;
}

static DiskBlock gen_random_block(){
  DiskBlock db;

  db.input = gen_random(100);
  db.bal = rand();
  db.mbal = rand();
  db.slot = rand();

  return db;
}

DiskTest::DiskTest(int k, int n_p, std::string disk, char * trid){
  srand( (unsigned) time(NULL) * getpid());
  int res = spdk_library_start(n_p,trid);

	if (res){
    std::cout << "An error occured while initializing SPDK" << std::endl;
    exit(-1);
  }

  this->k = k;
  this->NUM_PROCESSES = n_p;
  this->addresses = disk;

  std::future<void> f1 = initialize(this->addresses,this->k * this->NUM_PROCESSES,0);
	f1.get();
	std::cout << "initialize blocks completed" << std::endl;
}

bool DiskTest::single_write_read_test(void){
  DiskBlock db = gen_random_block();

  int row = rand() % this->k;
  int column = rand() % this->NUM_PROCESSES;

  int index = row * this->NUM_PROCESSES + column;

  std::future<void> f1 = write(this->addresses,db,row,column);
  f1.get();

  std::future<std::unique_ptr<DiskBlock>> f3 = read(this->addresses,index);
  auto db2 = f3.get();

  bool res = db.input.compare(db2->input) == 0 && db.bal == db2->bal && db.mbal == db2->mbal && db.slot == db2->slot;

  return res;
}

bool DiskTest::single_write_read_test(int row,int column){
  DiskBlock db = gen_random_block();

  int index = row * this->NUM_PROCESSES + column;

  std::future<void> f1 = write(this->addresses,db,row,column);
  f1.get();

  std::future<std::unique_ptr<DiskBlock>> f3 = read(this->addresses,index);
  auto db2 = f3.get();

  bool res = db.input.compare(db2->input) == 0 && db.bal == db2->bal && db.mbal == db2->mbal && db.slot == db2->slot;

  return res;
}

void DiskTest::random_read(int max_row,int max_column){
  int row = rand() % max_column;
  int column = rand() % max_column;

  int index = row * this->NUM_PROCESSES + column;

  std::future<std::unique_ptr<DiskBlock>> f3 = read(this->addresses,index);
  auto db2 = f3.get();
}

int DiskTest::multi_write_read_test(int number_of_tests){

  bool res = false;
  int k = 0;

  for (int i = 0; i < number_of_tests; i++) {
    res = this->single_write_read_test();

    if (res){
      k++;
    }
  }

  return k;
}

int DiskTest::multi_random_read_test(int number_of_tests){

  int k = 0;

  for (int i = 0; i < number_of_tests; i++) {
    this->random_read(100000,100000);
    k++;
  }

  return k;
}

void DiskTest::run_every_test(int number_of_tests){

  int res;

  std::cout << "Current config: Value k = " << this->k << " N_PROCESSES = " << this->NUM_PROCESSES << " N_TESTS = " << number_of_tests << std::endl;

  std::cout << "Running multi random reads" << std::endl;
  res = this->multi_random_read_test(number_of_tests);
  std::cout << "Passed " << res << " of " << number_of_tests << " Tests" << std::endl;

  std::cout << "Running multi random writes and then reads" << std::endl;
  res = this->multi_write_read_test(number_of_tests);
  std::cout << "Passed " << res << " of " << number_of_tests << " Tests" << std::endl;

}

DiskTest::~DiskTest(){
  spdk_library_end();
}
