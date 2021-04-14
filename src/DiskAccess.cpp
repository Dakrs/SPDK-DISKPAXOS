extern "C" {
	#include "spdk/stdinc.h"
	#include "spdk/nvme.h"
	#include "spdk/vmd.h"
	#include "spdk/nvme_zns.h"
	#include "spdk/env.h"
}

#include "DiskAccess.hpp"
#include <map>
#include <string>
#include <set>
#include <iostream>
#include <memory>
#include <vector>
#include <stdio.h>
#include <cstring>
#include <stdexcept>
#include <system_error>
#include <memory>

#define NUM_PROCESSES 2

typedef unsigned char byte;

struct NVME_CONTROLER {
  struct spdk_nvme_ctrlr *ctrlr;
  std::string name;

  NVME_CONTROLER(struct spdk_nvme_ctrlr *ctrlr,char * chr_name){
    ctrlr = ctrlr;
    name = std::string(chr_name);
  };
  ~NVME_CONTROLER(){
    std::cout << "Exiting CTRLR: " << name << std::endl;
  };
};

struct NVME_NAMESPACE {
  struct spdk_nvme_ctrlr	*ctrlr;
	struct spdk_nvme_ns	*ns;
	struct spdk_nvme_qpair	*qpair;
  uint32_t metadata_size;
  uint32_t lbaf;

  NVME_NAMESPACE(
    struct spdk_nvme_ctrlr	*ctrlr,
    struct spdk_nvme_ns	*ns
  ): ctrlr(ctrlr), ns(ns) {
    metadata_size = spdk_nvme_ns_get_md_size(ns);
    lbaf = spdk_nvme_ns_get_sector_size(ns);
  };
  ~NVME_NAMESPACE(){
    std::cout << "Exiting NAMESPACE: " << std::endl;
  };
};

template<class T>
struct CallBack {
  byte * buffer;
  std::string type;
  int k;
  int status;
  std::promise<T> callback;
	size_t size_per_elem;

  CallBack(
    byte * buffer,
    std::string type,
    int k
  ): buffer(buffer), type(type), k(k) {
    status = 0;
  };
	CallBack(
		byte * buffer,
		std::string type,
		int k,
		size_t size_per_elem
	): buffer(buffer), type(type), k(k), size_per_elem(size_per_elem) {
		status = 0;
	};
  ~CallBack(){};
};

std::set<std::string> addresses;
std::vector<std::unique_ptr<NVME_CONTROLER>> controllers;
std::map<std::string,std::unique_ptr<NVME_NAMESPACE>> namespaces;



static void string_to_bytes(std::string str, byte * buffer){
	int size = str.length();

	buffer[0] = size & 0x000000ff;
	buffer[1] = ( size & 0x0000ff00 ) >> 8;
 	buffer[2] = ( size & 0x00ff0000 ) >> 16;
 	buffer[3] = ( size & 0xff000000 ) >> 24;

	std::memcpy(buffer+4, str.data(), size);
}

static std::string bytes_to_string(byte * buffer){
	int size = int((unsigned char)(buffer[0]) |
            (unsigned char)(buffer[1]) << 8 |
            (unsigned char)(buffer[2]) << 16 |
            (unsigned char)(buffer[3]) << 24);

	return std::string((char *)buffer+4,size);
}

static bool probe_cb(
  void *cb_ctx,
  const struct spdk_nvme_transport_id *trid,
  struct spdk_nvme_ctrlr_opts *opts
){
  std::string addr(trid->traddr);
  const bool is_in = addresses.find(addr) != addresses.end();

  if (!is_in){
    addresses.insert(addr);
    std::cout << "Device on addr: " << addr << std::endl;
    return true;
  }
  return false;
}

static int
register_ns(
  struct spdk_nvme_ctrlr *ctrlr,
  struct spdk_nvme_ns *ns,
  const struct spdk_nvme_transport_id *trid
){
  if (!spdk_nvme_ns_is_active(ns)) {
		return -1;
	}

  NVME_NAMESPACE * my_ns = new NVME_NAMESPACE(ctrlr,ns);
  my_ns->qpair = spdk_nvme_ctrlr_alloc_io_qpair(ctrlr, NULL, 0);
  if (my_ns->qpair == NULL){
    std::cout << "ERROR: spdk_nvme_ctrlr_alloc_io_qpair() failed" << std::endl;
    return -1;
  }

  std::string addr(trid->traddr);
  namespaces.insert(std::pair<std::string,std::unique_ptr<NVME_NAMESPACE>>(addr,std::unique_ptr<NVME_NAMESPACE>(my_ns)));

  printf("  Namespace ID: %d size: %juGB %lu\n", spdk_nvme_ns_get_id(ns),
         spdk_nvme_ns_get_size(ns) / 1000000000, spdk_nvme_ns_get_num_sectors(ns));

  return 0;
}

static void
attach_cb(
  void *cb_ctx,
  const struct spdk_nvme_transport_id *trid,
	struct spdk_nvme_ctrlr *ctrlr,
  const struct spdk_nvme_ctrlr_opts *opts
){
  struct spdk_nvme_ns * ns;
	const struct spdk_nvme_ctrlr_data *cdata;

  //getting controller data
  cdata = spdk_nvme_ctrlr_get_data(ctrlr);
  char * chr_name = new char[1024];

  snprintf(chr_name, sizeof(char)*1024, "%-20.20s (%-20.20s)", cdata->mn, cdata->sn);

  NVME_CONTROLER * current_ctrlr = new NVME_CONTROLER(ctrlr,chr_name);

  delete chr_name;

  int nsid, num_ns;
  num_ns = spdk_nvme_ctrlr_get_num_ns(ctrlr);

  for (nsid = 1; nsid <= num_ns; nsid++) {
		ns = spdk_nvme_ctrlr_get_ns(ctrlr, nsid);
		if (ns == NULL) {
			continue;
		}
		int res = register_ns(ctrlr, ns, trid);
    if (!res)
      break; // só quero o primeiro namespace
	}

  controllers.push_back(std::unique_ptr<NVME_CONTROLER>(current_ctrlr));
}

static void cleanup(void){
  struct spdk_nvme_detach_ctx *detach_ctx = NULL;

  for(const auto& ns_entry : namespaces){
    const std::unique_ptr<NVME_NAMESPACE>& ns = ns_entry.second;
    spdk_nvme_ctrlr_free_io_qpair(ns->qpair);
  }

  for(auto&& ctrl : controllers){
    spdk_nvme_detach_async(ctrl->ctrlr, &detach_ctx);
  }

  while (detach_ctx && spdk_nvme_detach_poll_async(detach_ctx) == -EAGAIN);
}

int spdk_library_start(void){
  struct spdk_env_opts opts;
  spdk_env_opts_init(&opts);

  opts.name = "Disk Paxos";
  opts.shm_id = 0; // não sei o que faz esta opção

  if (spdk_env_init(&opts) < 0) {
    fprintf(stderr, "Unable to initialize SPDK env\n");
    return 1;
  }

  if (spdk_vmd_init()) {
    fprintf(stderr, "Failed to initialize VMD."
      " Some NVMe devices can be unavailable.\n");
  }

  int rc = spdk_nvme_probe(NULL, NULL, probe_cb, attach_cb, NULL);
  if (rc != 0) {
    fprintf(stderr, "spdk_nvme_probe() failed\n");
    //cleanup();
    return 1;
  }

  spdk_vmd_fini();
  return 0;
}

void spdk_library_end(){
  cleanup();
  spdk_vmd_fini();
}

/**
Callback function call using spdk_nvme_qpair_process_completions after a write on a block.
*/

static void write_complete(void *arg,const struct spdk_nvme_cpl *completion){
  CallBack<void> * cb = (CallBack<void> *) arg;

  if (spdk_nvme_cpl_is_error(completion)) {
    fprintf(stderr, "I/O error status: %s\n", spdk_nvme_cpl_get_status_string(&completion->status));
    fprintf(stderr, "Write I/O failed, aborting run\n");
    cb->status = 2;
    exit(1);
  }

  spdk_free(cb->buffer);
  cb->status = 1;
  //delete cb;
  cb->callback.set_value();
}

std::future<void> write(std::string disk, DiskBlock& db,int k,int p_id){
  std::map<std::string,std::unique_ptr<NVME_NAMESPACE>>::iterator it;
  it = namespaces.find(disk);

  if (it == namespaces.end()){
    throw std::invalid_argument( "Disk not found" );
  }

  int LBA_INDEX = k * NUM_PROCESSES + p_id;

  size_t BUFFER_SIZE = (it->second->lbaf + it->second->metadata_size);

  std::string db_serialized = db.serialize();

  byte * buffer = (byte *) spdk_zmalloc(BUFFER_SIZE, 0x1000, NULL, SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);

  if (buffer == NULL) {
    throw std::bad_alloc();
  }

  string_to_bytes(db_serialized,buffer);

  CallBack<void> * cb = new CallBack<void>(buffer,"WRITE",k);
  cb->callback = std::promise<void>();
  auto future = cb->callback.get_future();

  int rc = spdk_nvme_ns_cmd_write(it->second->ns, it->second->qpair, buffer,
            LBA_INDEX, /* LBA start */
            1, /* number of LBAs */
            write_complete, cb, 0);

  if (rc != 0) {
    fprintf(stderr, "starting write I/O failed\n");
    exit(1);
  }

  while (!cb->status) {
    spdk_nvme_qpair_process_completions(it->second->qpair, 0);
  }
  //delete cb;
  return future;
}


static void read_complete(void *arg, const struct spdk_nvme_cpl *completion){
  CallBack<std::unique_ptr<DiskBlock>> * cb = (CallBack<std::unique_ptr<DiskBlock>> *) arg;

  if (spdk_nvme_cpl_is_error(completion)) {
    fprintf(stderr, "I/O error status: %s\n", spdk_nvme_cpl_get_status_string(&completion->status));
    fprintf(stderr, "Write I/O failed, aborting run\n");
    cb->status = 2;
    exit(1);
  }

  cb->status = 1; //completed with success
  std::string db_serialized = bytes_to_string(cb->buffer); // remove string from buffer.

  DiskBlock * db = new DiskBlock();
  db->deserialize(db_serialized);
  cb->callback.set_value(std::unique_ptr<DiskBlock>(db));

  spdk_free(cb->buffer);
}

std::future<std::unique_ptr<DiskBlock>> read(std::string disk,int k,int p_id){
  std::map<std::string,std::unique_ptr<NVME_NAMESPACE>>::iterator it;

  it = namespaces.find(disk);
  if (it == namespaces.end()){
    throw std::invalid_argument( "Disk not found" );
  }

  int LBA_INDEX = k * NUM_PROCESSES + p_id;
  size_t BUFFER_SIZE = (it->second->lbaf + it->second->metadata_size);

  byte * buffer = (byte *) spdk_zmalloc(BUFFER_SIZE, 0x1000, NULL, SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);

  if (buffer == NULL) {
    throw std::bad_alloc();
  }

  CallBack<std::unique_ptr<DiskBlock>> * cb = new CallBack<std::unique_ptr<DiskBlock>>(buffer,"READ",k);
  cb->callback = std::promise<std::unique_ptr<DiskBlock>>();
  auto future = cb->callback.get_future();

  int rc = spdk_nvme_ns_cmd_read(it->second->ns, it->second->qpair, buffer,
           LBA_INDEX, /* LBA start */
           1, /* number of LBAs */
           read_complete, cb, 0);

  if (rc != 0) {
      fprintf(stderr, "starting write I/O failed\n");
      exit(1);
  }

  while (!cb->status) {
      spdk_nvme_qpair_process_completions(it->second->qpair, 0);
  }
  //delete cb;

  return future;
}

static void read_full_complete(void *arg, const struct spdk_nvme_cpl *completion){
  CallBack<std::unique_ptr<std::vector<std::unique_ptr<DiskBlock>>>> * cb = (CallBack<std::unique_ptr<std::vector<std::unique_ptr<DiskBlock>>>> *) arg;

  if (spdk_nvme_cpl_is_error(completion)) {
    fprintf(stderr, "I/O error status: %s\n", spdk_nvme_cpl_get_status_string(&completion->status));
    fprintf(stderr, "Write I/O failed, aborting run\n");
    cb->status = 2;
    exit(1);
  }

  cb->status = 1;
	auto vec = new std::vector<std::unique_ptr<DiskBlock>>();
	int size_elem = cb->size_per_elem;
	for(int i = 0; i < NUM_PROCESSES; i++){
		std::string db_serialized = bytes_to_string(cb->buffer + size_elem * i);
		DiskBlock * db = new DiskBlock();
	  db->deserialize(db_serialized);

		vec->push_back(std::unique_ptr<DiskBlock>(db));
	}
	cb->callback.set_value(std::unique_ptr<std::vector<std::unique_ptr<DiskBlock>>>(vec));

  spdk_free(cb->buffer);
}

std::future<std::unique_ptr<std::vector<std::unique_ptr<DiskBlock>>>> read_full(std::string disk,int k){
  std::map<std::string,std::unique_ptr<NVME_NAMESPACE>>::iterator it;

  it = namespaces.find(disk);
  if (it == namespaces.end()){
		throw std::invalid_argument( "Disk not found" );
  }

  int LBA_INDEX = k * NUM_PROCESSES;
  size_t BUFFER_SIZE = (it->second->lbaf + it->second->metadata_size);

  byte * buffer = (byte *) spdk_zmalloc(BUFFER_SIZE * NUM_PROCESSES, 0x1000, NULL, SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);

	if (buffer == NULL) {
		throw std::bad_alloc();
	}

  CallBack<std::unique_ptr<std::vector<std::unique_ptr<DiskBlock>>>> * cb = new CallBack<std::unique_ptr<std::vector<std::unique_ptr<DiskBlock>>>>(buffer,"READ",k,BUFFER_SIZE);
	cb->callback = std::promise< std::unique_ptr<std::vector<std::unique_ptr<DiskBlock>>> >();
  auto future = cb->callback.get_future();

  int rc = spdk_nvme_ns_cmd_read(it->second->ns, it->second->qpair, buffer,
           LBA_INDEX, /* LBA start */
           NUM_PROCESSES, /* number of LBAs */
           read_full_complete, cb, 0);

  if (rc != 0) {
      fprintf(stderr, "starting write I/O failed\n");
      exit(1);
  }

  while (!cb->status) {
      spdk_nvme_qpair_process_completions(it->second->qpair, 0);
  }
  //delete cb;

	return future;
}
