#ifndef DISKISOMORPHIC_TEST_H
#define DISKISOMORPHIC_TEST_H

#include <string>

class DiskTest {
  int k;
  int NUM_PROCESSES;
  std::string addresses;

  public:
    DiskTest(int k, int n_p,std::string disk,char * trid);
    ~DiskTest();
    bool single_write_read_test(void);
    bool single_write_read_test(int row,int column);
    void random_read(int max_row,int max_column);
    int multi_write_read_test(int number_of_tests);
    void run_every_test(int number_of_tests);
    int multi_random_read_test(int number_of_tests);
};

#endif
