extern "C" {
	#include "spdk/event.h"
	#include "spdk/stdinc.h"
	#include "spdk/nvme.h"
	#include "spdk/vmd.h"
	#include "spdk/nvme_zns.h"
	#include "spdk/env.h"
	#include "spdk/thread.h"
}

#include "Disk/SPDK_ENV.hpp"
#include "Disk/SglOpts.hpp"
#include "Disk/DiskBlock.hpp"
#include <string>
#include <memory>
#include <map>
#include <stdio.h>
#include <iostream>

#define MAX_IOVS 32

static int io_complete_flag = 0;

enum TypeSGL { Data = 0, BitBucket = 1 };

struct sgl_element {
	void *base;
	size_t offset;
	size_t len;
  TypeSGL type;
};

struct io_request{
	uint32_t current_iov_index;
	uint32_t current_iov_bytes_left;
	struct sgl_element iovs[MAX_IOVS];
	uint32_t nseg;

  io_request(size_t len,uint32_t nsegs){
    this->current_iov_index = 0;
    this->current_iov_bytes_left = 0;
    this->nseg = nsegs;

    for(uint32_t i = 0; i < this->nseg; i++){
      if ((i % SPDK_ENV::NUM_PROCESSES) == 0){
        this->iovs[i].base = spdk_zmalloc(len, 0x1000, NULL, SPDK_ENV_LCORE_ID_ANY, SPDK_MALLOC_DMA);
        this->iovs[i].type = TypeSGL::Data;
      }
      else{
        this->iovs[i].base = (void *)UINT64_MAX;
        this->iovs[i].type = TypeSGL::BitBucket;
      }
      this->iovs[i].len = len;
    }
  };

  ~io_request(){
    for(uint32_t i = 0; i < this->nseg; i++){
      if (this->iovs[i].type == TypeSGL::Data)
        spdk_free(this->iovs[i].base);
    }
  };
};


static void nvme_request_reset_sgl(void *cb_arg, uint32_t sgl_offset)
{
	uint32_t i;
	uint32_t offset = 0;
	struct sgl_element *iov;
	struct io_request *req = (struct io_request *)cb_arg;

	for (i = 0; i < req->nseg; i++) {
		iov = &req->iovs[i];
		offset += iov->len;
		if (offset > sgl_offset) {
			break;
		}
	}
	req->current_iov_index = i;
	req->current_iov_bytes_left = offset - sgl_offset;

  std::cout << "reset called: " << req->current_iov_bytes_left << " offset: " << offset << " sgl_offset: "  << sgl_offset << '\n';
	return;
}

static int nvme_request_next_sge(void *cb_arg, void **address, uint32_t *length)
{
	struct io_request *req = (struct io_request *)cb_arg;
	struct sgl_element *iov;

	if (req->current_iov_index >= req->nseg) {
		*length = 0;
		*address = NULL;
		return 0;
	}

	iov = &req->iovs[req->current_iov_index];

	if (req->current_iov_bytes_left) {
		*address = iov->base + iov->offset + iov->len - req->current_iov_bytes_left;
		*length = req->current_iov_bytes_left;
		req->current_iov_bytes_left = 0;
	} else {
		*address = iov->base + iov->offset;
		*length = iov->len;
	}

	req->current_iov_index++;

	return 0;
}

static void io_complete(void *ctx, const struct spdk_nvme_cpl *cpl)
{
	if (spdk_nvme_cpl_is_error(cpl)) {
		io_complete_flag = 2;
	} else {
		io_complete_flag = 1;
	}
}

static void string_to_bytes(std::string str, unsigned char * buffer){
	int size = str.length();
	//é preciso dar throw a um erro caso o tamanho da string seja superior ao tamanho dos blocos.

	buffer[0] = size & 0x000000ff;
	buffer[1] = ( size & 0x0000ff00 ) >> 8;
 	buffer[2] = ( size & 0x00ff0000 ) >> 16;
 	buffer[3] = ( size & 0xff000000 ) >> 24;

	std::memcpy(buffer+4, str.data(), size);
}

namespace SglOpts {
  void basic_write(uint32_t BASE_LBA){
    uint32_t core = SPDK_ENV::allocate_replica_core();
    using namespace std;

    map<string,unique_ptr<SPDK_ENV::NVME_NAMESPACE_MULTITHREAD>>::iterator it;
  	map<uint32_t,struct spdk_nvme_qpair	*>::iterator it_qpair;

    for(auto disk_id: SPDK_ENV::addresses){
  		it = SPDK_ENV::namespaces.find(disk_id);

  		if (it != SPDK_ENV::namespaces.end()){

  			size_t BUFFER_SIZE = (it->second->info.lbaf + it->second->info.metadata_size);
        io_request * req = new io_request(BUFFER_SIZE,12);
        for (uint32_t i = 0; i < 4; i++) {
          DiskBlock db;
          db.slot = i;
          db.input = "hello_world";
          string local_db_serialized = db.serialize();
          string_to_bytes(local_db_serialized,(unsigned char *)req->iovs[(int)(i*3)].base);
        }

  			it_qpair = it->second->qpairs.find(core);

        int rc = spdk_nvme_ns_cmd_writev(it->second->ns, it_qpair->second, BASE_LBA, 12,
				     io_complete, req, 0,
				     nvme_request_reset_sgl,
				     nvme_request_next_sge);

  			if (rc != 0) {
  				SPDK_ENV::error_on_cmd_submit(rc,"basic sgl write","write");
  				exit(1);
  			}
  		}
  	}

    while (!io_complete_flag) {
  		spdk_nvme_qpair_process_completions(it_qpair->second, 1);
  	}

    std::cout << "sgl write completed" << std::endl;
  }
}
