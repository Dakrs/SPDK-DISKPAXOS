#ifndef SPDK_ENV_HPP
#define SPDK_ENV_HPP

extern "C" {
	#include "spdk/event.h"
	#include "spdk/stdinc.h"
	#include "spdk/nvme.h"
	#include "spdk/vmd.h"
	#include "spdk/nvme_zns.h"
	#include "spdk/env.h"
	#include "spdk/thread.h"
}

#include <cstring>
#include <stdio.h>
#include <map>
#include <string>
#include <set>
#include <memory>
#include <vector>
#include <iostream>

namespace SPDK_ENV {

  extern int NUM_PROCESSES;
  extern int NUM_CONCENSOS_LANES;
  extern uint32_t NEXT_CORE;
  extern uint32_t NEXT_CORE_REPLICA;

  struct NVME_CONTROLER_V2 {
    struct spdk_nvme_ctrlr *ctrlr;
    std::string name;

    NVME_CONTROLER_V2(struct spdk_nvme_ctrlr * ctrlr,char * chr_name){
      this->ctrlr = ctrlr;
      name = std::string(chr_name);
    };
    ~NVME_CONTROLER_V2(){
      std::cout << "Exiting CTRLR: " << name << std::endl;
    };
  };

  struct NVME_NAMESPACE_INFO {
    uint32_t lbaf; // size in bytes of each block
  	uint32_t metadata_size; // bytes used by metadata of each block

    NVME_NAMESPACE_INFO(uint32_t lbaf,uint32_t metadata_size): lbaf(lbaf), metadata_size(metadata_size){};
    ~NVME_NAMESPACE_INFO(){};
  };

  struct NVME_NAMESPACE_MULTITHREAD {
    struct spdk_nvme_ctrlr	*ctrlr;
  	struct spdk_nvme_ns	*ns;
  	std::map<uint32_t,struct spdk_nvme_qpair	*> qpairs;
  	NVME_NAMESPACE_INFO info;

    NVME_NAMESPACE_MULTITHREAD(
      struct spdk_nvme_ctrlr	*ctrlr,
      struct spdk_nvme_ns	*ns
    ): ctrlr(ctrlr), ns(ns), info(NVME_NAMESPACE_INFO( spdk_nvme_ns_get_sector_size(ns), spdk_nvme_ns_get_md_size(ns))) {};
    ~NVME_NAMESPACE_MULTITHREAD(){
      std::cout << "Exiting NAMESPACE: " << std::endl;
    };
  };


  extern std::set<std::string> addresses;
  extern std::vector<std::unique_ptr<NVME_CONTROLER_V2>> controllers;
  extern std::map<std::string,std::unique_ptr<NVME_NAMESPACE_MULTITHREAD>> namespaces;

  int spdk_start(int n_p,int n_k);
  void spdk_end();
  uint32_t allocate_leader_core();
  uint32_t allocate_replica_core();

}

#endif