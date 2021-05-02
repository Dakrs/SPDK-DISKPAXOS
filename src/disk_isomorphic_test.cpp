#include "Disk/DiskBlock.hpp"
#include "Disk/DiskAccess.hpp"
#include "Test/disk_isomorphic_test.hpp"
#include <iostream>
#include <ctime>
#include <unistd.h>
#include <random>
#include <stdlib.h>

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

DiskTest::DiskTest(int k, int n_p){
  srand( (unsigned) time(NULL) * getpid());
  int res = spdk_library_start(n_p);

	if (res){
    std::cout << "An error occured while initializing SPDK" << std::endl;
    exit(-1);
  }

  this->k = k;
  this->NUM_PROCESSES = n_p;

  std::future<void> f1 = initialize("0000:03:00.0",this->k);
	f1.get();
	std::cout << "initialize blocks completed" << std::endl;
}

bool DiskTest::single_write_read_test(void){
  DiskBlock db = gen_random_block();

  int row = rand() % this->k;
  int column = rand() % this->NUM_PROCESSES;

  std::future<void> f1 = write("0000:03:00.0",db,row,column);
  f1.get();

  std::future<std::unique_ptr<DiskBlock>> f3 = read("0000:03:00.0",row,column);
  auto db2 = f3.get();

  bool res = db.input.compare(db2->input) == 0 && db.bal == db2->bal && db.mbal == db2->mbal && db.slot == db2->slot;

  return res;
}

int DiskTest::multi_write_read_test(int number_of_tests){

  bool res = false;
  int k = 0;

  for (int i = 0; i < number_of_tests; i++) {
    res = this->single_write_read_test();

    if (res)
      k++;
  }

  return k;
}

void DiskTest::run_every_test(int number_of_tests){

  std::cout << "Running multi random writes and reads" << std::endl;
  std::cout << "Current config: Value k = " << this->k << " N_PROCESSES = " << this->NUM_PROCESSES << " N_TESTS = " << number_of_tests << std::endl;

  int res = this->multi_write_read_test(number_of_tests);

  std::cout << "Passed " << res << " of " << number_of_tests << " Tests" << std::endl;
}

DiskTest::~DiskTest(){
  spdk_library_end();
}
