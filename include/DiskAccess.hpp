#ifndef DISKACCESS_H
#define DISKACCESS_H

#include "DiskBlock.hpp"
#include <string>
#include <future>
#include <memory>
#include <vector>

int spdk_library_start(void);
void spdk_library_end(void);
std::future<void> write(std::string disk, DiskBlock& db,int k,int p_id);
std::future<std::unique_ptr<DiskBlock>> read(std::string disk, int k,int p_id);
std::future<std::unique_ptr<std::vector<std::unique_ptr<DiskBlock>>>> read_full(std::string disk,int k);
#endif
