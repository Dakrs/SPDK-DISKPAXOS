extern "C" {
	#include "spdk/event.h"
	#include "spdk/stdinc.h"
	#include "spdk/nvme.h"
	#include "spdk/vmd.h"
	#include "spdk/nvme_zns.h"
	#include "spdk/env.h"
	#include "spdk/thread.h"
}

#include "Disk/DiskAccess.hpp"
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
#include <thread>
#include <atomic>

typedef unsigned char byte;

int NUM_PROCESSES = 0;

/**
Struct used to keep track of all controllers found.
*/

struct NVME_CONTROLER {
  struct spdk_nvme_ctrlr *ctrlr;
  std::string name;

  NVME_CONTROLER(struct spdk_nvme_ctrlr * ctrlr,char * chr_name){
    this->ctrlr = ctrlr;
    name = std::string(chr_name);
  };
  ~NVME_CONTROLER(){
    std::cout << "Exiting CTRLR: " << name << std::endl;
  };
};

/**
Struct used to keep track of the namespace used in each controller.
*/

struct NVME_NAMESPACE {
  struct spdk_nvme_ctrlr	*ctrlr;
	struct spdk_nvme_ns	*ns;
	struct spdk_nvme_qpair	*qpair;
  uint32_t metadata_size;
  uint32_t lbaf;
	int internal_id;

  NVME_NAMESPACE(
    struct spdk_nvme_ctrlr	*ctrlr,
    struct spdk_nvme_ns	*ns,
		int internal_id
  ): ctrlr(ctrlr), ns(ns), internal_id(internal_id) {
    metadata_size = spdk_nvme_ns_get_md_size(ns);
    lbaf = spdk_nvme_ns_get_sector_size(ns);
  };
  ~NVME_NAMESPACE(){
    std::cout << "Exiting NAMESPACE: " << std::endl;
  };
};


struct CallBackOpts {
	int LBA_INDEX;
	int LBA_SIZE;
	size_t size_per_elem;

	CallBackOpts(){
		LBA_INDEX = 0;
		size_per_elem = 0;
		LBA_SIZE = 0;
	}
	CallBackOpts(int LBA_VALUE,size_t size_per){
		LBA_INDEX = LBA_VALUE;
		size_per_elem = size_per;
		LBA_SIZE = 0;
	};
	CallBackOpts(int LBA_VALUE){
		LBA_INDEX = LBA_VALUE;
		size_per_elem = 0;
		LBA_SIZE = 0;
	};
	~CallBackOpts(){};
};

/**
class used for event callbacks
*/

template<class T>
struct CallBack {
  byte * buffer;
  std::string disk;
  int status;
  std::promise<T> callback;
	CallBackOpts opts;

  CallBack(
    byte * buffer,
    std::string disk_id,
    int k
  ): buffer(buffer), disk(disk_id) {
    status = 0;
		opts = CallBackOpts(k);
  };
	CallBack(
		byte * buffer,
		std::string disk_id
	): buffer(buffer), disk(disk_id) {
		status = 0;
		opts = CallBackOpts();
	};
	CallBack(
		byte * buffer,
		std::string disk_id,
		int k,
		size_t size_per_elem
	): buffer(buffer), disk(disk_id){
		status = 0;
		opts = CallBackOpts(k,size_per_elem);
	};
  ~CallBack(){};
};

std::set<std::string> addresses; //set with all Controller IP found
std::vector<std::unique_ptr<NVME_CONTROLER>> controllers; //a list with all controllers found
std::map<std::string,std::unique_ptr<NVME_NAMESPACE>> namespaces; //a map (ip,namespace) for all namespace used

std::atomic<bool> ready (false);
std::thread internal_spdk_event_launcher;

/**
Function to write a encode byte block into a buffer;
the first 4 bytes keep the size of the block.
*/

static void string_to_bytes(std::string str, byte * buffer){
	int size = str.length();

	//é preciso dar throw a um erro caso o tamanho da string seja superior ao tamanho dos blocos.

	buffer[0] = size & 0x000000ff;
	buffer[1] = ( size & 0x0000ff00 ) >> 8;
 	buffer[2] = ( size & 0x00ff0000 ) >> 16;
 	buffer[3] = ( size & 0xff000000 ) >> 24;

	std::memcpy(buffer+4, str.data(), size);
}

/**
Function to read a byte buffer and return the byte string it contains.
*/

static std::string bytes_to_string(byte * buffer){
	int size = int((unsigned char)(buffer[0]) |
            (unsigned char)(buffer[1]) << 8 |
            (unsigned char)(buffer[2]) << 16 |
            (unsigned char)(buffer[3]) << 24);

	if (size > 512){
		return std::string((char *)buffer,0);
	}


	return std::string((char *)buffer+4,size);
}

static bool probe_cb(void *cb_ctx,const struct spdk_nvme_transport_id *trid,struct spdk_nvme_ctrlr_opts *opts){
  std::string addr(trid->traddr);
  const bool is_in = addresses.find(addr) != addresses.end();

  if (!is_in){
    addresses.insert(addr);
    std::cout << "Device on addr: " << addr << std::endl;
    return true;
  }
  return false;
}

static int register_ns(struct spdk_nvme_ctrlr *ctrlr,struct spdk_nvme_ns *ns,const struct spdk_nvme_transport_id *trid){
  if (!spdk_nvme_ns_is_active(ns)) {
		return -1;
	}

  NVME_NAMESPACE * my_ns = new NVME_NAMESPACE(ctrlr,ns,namespaces.size());
  my_ns->qpair = spdk_nvme_ctrlr_alloc_io_qpair(ctrlr, NULL, 0);
  if (my_ns->qpair == NULL){
    std::cout << "ERROR: spdk_nvme_ctrlr_alloc_io_qpair() failed" << std::endl;
    return -1;
  }

  std::string addr(trid->traddr);
  namespaces.insert(std::pair<std::string,std::unique_ptr<NVME_NAMESPACE>>(addr,std::unique_ptr<NVME_NAMESPACE>(my_ns)));

  printf("  Namespace ID: %d size: %juGB %lu i_id: %d\n", spdk_nvme_ns_get_id(ns),
         spdk_nvme_ns_get_size(ns) / 1000000000, spdk_nvme_ns_get_num_sectors(ns), my_ns->internal_id);

  return 0;
}

static void attach_cb( void *cb_ctx, const struct spdk_nvme_transport_id *trid, struct spdk_nvme_ctrlr *ctrlr, const struct spdk_nvme_ctrlr_opts *opts){
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

  for(auto& ctrl : controllers){
    spdk_nvme_detach_async(ctrl->ctrlr, &detach_ctx);
  }

  while (detach_ctx && spdk_nvme_detach_poll_async(detach_ctx) == -EAGAIN){
		;
	}
}

static void app_init(void * arg){
	/**
	if (spdk_vmd_init()) {
		fprintf(stderr, "Failed to initialize VMD."
			" Some NVMe devices can be unavailable.\n");
	}*/

	int rc = spdk_nvme_probe(NULL, NULL, probe_cb, attach_cb, NULL);
	if (rc != 0) {
		fprintf(stderr, "spdk_nvme_probe() failed\n");
		cleanup();
		exit(-1);
	}

	ready = true;

	std::cout << "Succeded: " << spdk_env_get_core_count() << std::endl;
}

static void run_spdk_event_framework(){
  struct spdk_app_opts app_opts = {};
  spdk_app_opts_init(&app_opts, sizeof(app_opts));
	app_opts.name = "event_test";
	app_opts.reactor_mask = "0x2f";

  int rc = spdk_app_start(&app_opts, app_init, NULL);

	if (rc){
		std::cout << "Error might have occured" << std::endl;
	}

	cleanup();
	//spdk_vmd_fini();

	spdk_app_fini();
}

int spdk_library_start(int n_p) {

	NUM_PROCESSES = n_p;

	internal_spdk_event_launcher = std::thread(run_spdk_event_framework);

	while(!ready);

  return 0;
}

void spdk_library_end() {
	spdk_app_stop(0);
	internal_spdk_event_launcher.join();
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
  cb->callback.set_value();
}

/**
Verify if an event has already been completed and trigger callback.
*/
template<class T>
static void verify_event(void * arg1, void * arg2){
	CallBack<T> * cb = (CallBack<T> *) arg1;
	std::map<std::string,std::unique_ptr<NVME_NAMESPACE>>::iterator it;
  it = namespaces.find(cb->disk);

	if(!cb->status){
		spdk_nvme_qpair_process_completions(it->second->qpair, 0);

		uint32_t target_core = it->second->internal_id / spdk_env_get_core_count();

		struct spdk_event * e = spdk_event_allocate(target_core,verify_event<T>,cb,NULL);
		spdk_event_call(e);
	}
	else{
		delete cb;
	}
}

/**
Create create a write event
*/
static void write_event(void * arg1, void * arg2){
	CallBack<void> * cb = (CallBack<void> *) arg1;

	std::map<std::string,std::unique_ptr<NVME_NAMESPACE>>::iterator it;
  it = namespaces.find(cb->disk);

	int rc = spdk_nvme_ns_cmd_write(it->second->ns, it->second->qpair, cb->buffer,
						cb->opts.LBA_INDEX, /* LBA start */
						1, /* number of LBAs */
						write_complete, cb, SPDK_NVME_IO_FLAGS_FORCE_UNIT_ACCESS); //submit a write operation to NVME

	if (rc != 0) {
				fprintf(stderr, "starting write I/O failed\n");
				exit(1);
	}

	uint32_t target_core = it->second->internal_id / spdk_env_get_core_count(); //compute the core where the event should run in order that only 1 thread accesses the NVME QUEUE

	struct spdk_event * e = spdk_event_allocate(target_core,verify_event<void>,cb,NULL);
	spdk_event_call(e);
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

  CallBack<void> * cb = new CallBack<void>(buffer,disk,LBA_INDEX);
  cb->callback = std::promise<void>();
  auto future = cb->callback.get_future();

	uint32_t target_core = it->second->internal_id / spdk_env_get_core_count();

	struct spdk_event * e = spdk_event_allocate(target_core,write_event,cb,NULL);

	spdk_event_call(e);

  //delete cb; falta o delete do objeto
  return future;
}


/**
Event to initialize all disk blocks
*/
static void initialize_event(void * arg1, void * arg2){
	CallBack<void> * cb = (CallBack<void> *) arg1;

	std::map<std::string,std::unique_ptr<NVME_NAMESPACE>>::iterator it;
  it = namespaces.find(cb->disk);

	int rc = spdk_nvme_ns_cmd_write(it->second->ns, it->second->qpair, cb->buffer,
						cb->opts.LBA_INDEX, /* LBA start */
						cb->opts.LBA_SIZE, /* number of LBAs */
						write_complete, cb, SPDK_NVME_IO_FLAGS_FORCE_UNIT_ACCESS); //submit a write operation to NVME

	if (rc != 0) {
				fprintf(stderr, "starting write I/O failed\n");
				exit(1);
	}

	uint32_t target_core = it->second->internal_id / spdk_env_get_core_count(); //compute the core where the event should run in order that only 1 thread accesses the NVME QUEUE

	struct spdk_event * e = spdk_event_allocate(target_core,verify_event<void>,cb,NULL);
	spdk_event_call(e);
}

std::future<void> initialize(std::string disk, int size,int offset){
  std::map<std::string,std::unique_ptr<NVME_NAMESPACE>>::iterator it;
  it = namespaces.find(disk);

  if (it == namespaces.end()){
    throw std::invalid_argument( "Disk not found" );
  }

  size_t BUFFER_SIZE = (it->second->lbaf + it->second->metadata_size);

	/**
	DiskBlock db = DiskBlock();
  std::string db_serialized = db.serialize();*/

  byte * buffer = (byte *) spdk_zmalloc(BUFFER_SIZE * size, 0x1000, NULL, SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);

  if (buffer == NULL) {
    throw std::bad_alloc();
  }

	/**
	for (int i = 0; i < size; i++) {
		string_to_bytes(db_serialized,buffer + i * BUFFER_SIZE);
	}*/

  CallBack<void> * cb = new CallBack<void>(buffer,disk);
	cb->opts.LBA_INDEX = offset;
	cb->opts.LBA_SIZE = size;
  cb->callback = std::promise<void>();
  auto future = cb->callback.get_future();

	uint32_t target_core = it->second->internal_id / spdk_env_get_core_count();

	struct spdk_event * e = spdk_event_allocate(target_core,initialize_event,cb,NULL);

	spdk_event_call(e);

  //delete cb; falta o delete do objeto
  return future;
}

/**
Callback for the completion of a block read
*/

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

/**
Create an event to submit a read for a specify block to a NVME device
*/

static void read_event(void * arg1, void * arg2){
	CallBack<std::unique_ptr<DiskBlock>> * cb = (CallBack<std::unique_ptr<DiskBlock>> *) arg1;

	std::map<std::string,std::unique_ptr<NVME_NAMESPACE>>::iterator it;
  it = namespaces.find(cb->disk);

	int rc = spdk_nvme_ns_cmd_read(it->second->ns, it->second->qpair, cb->buffer,
					 cb->opts.LBA_INDEX, /* LBA start */
					 1, /* number of LBAs */
					 read_complete, cb, 0);

	if (rc != 0) {
				fprintf(stderr, "starting write I/O failed\n");
				exit(1);
	}

	uint32_t target_core = it->second->internal_id / spdk_env_get_core_count(); //compute the core where the event should run in order that only 1 thread accesses the NVME QUEUE

	struct spdk_event * e = spdk_event_allocate(target_core,verify_event<std::unique_ptr<DiskBlock>>,cb,NULL);
	spdk_event_call(e);
}


std::future<std::unique_ptr<DiskBlock>> read(std::string disk,int index){
  std::map<std::string,std::unique_ptr<NVME_NAMESPACE>>::iterator it;

  it = namespaces.find(disk);
  if (it == namespaces.end()){
    throw std::invalid_argument( "Disk not found" );
  }

  int LBA_INDEX = index;
  size_t BUFFER_SIZE = (it->second->lbaf + it->second->metadata_size);

  byte * buffer = (byte *) spdk_zmalloc(BUFFER_SIZE, 0x1000, NULL, SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);

  if (buffer == NULL) {
    throw std::bad_alloc();
  }

  CallBack<std::unique_ptr<DiskBlock>> * cb = new CallBack<std::unique_ptr<DiskBlock>>(buffer,disk,LBA_INDEX);
  cb->callback = std::promise<std::unique_ptr<DiskBlock>>();
  auto future = cb->callback.get_future();

	uint32_t target_core = it->second->internal_id / spdk_env_get_core_count();

	struct spdk_event * e = spdk_event_allocate(target_core,read_event,cb,NULL);
	spdk_event_call(e);
  //delete cb;

  return future;
}

/**
Callback for the completion of a full read row
*/

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
	int size_elem = cb->opts.size_per_elem;
	for(int i = 0; i < NUM_PROCESSES; i++){
		std::string db_serialized = bytes_to_string(cb->buffer + size_elem * i);
		DiskBlock * db = new DiskBlock();
	  db->deserialize(db_serialized);

		vec->push_back(std::unique_ptr<DiskBlock>(db));
	}
	cb->callback.set_value(std::unique_ptr<std::vector<std::unique_ptr<DiskBlock>>>(vec));

  spdk_free(cb->buffer);
}

/**
Create an event to submit a read for a full row to a NVME device
*/

static void read_full_event(void * arg1, void * arg2){
	CallBack<std::unique_ptr<std::vector<std::unique_ptr<DiskBlock>>>> * cb = (CallBack<std::unique_ptr<std::vector<std::unique_ptr<DiskBlock>>>> *) arg1;

	std::map<std::string,std::unique_ptr<NVME_NAMESPACE>>::iterator it;
  it = namespaces.find(cb->disk);

	int rc = spdk_nvme_ns_cmd_read(it->second->ns, it->second->qpair, cb->buffer,
					 cb->opts.LBA_INDEX, /* LBA start */
					 NUM_PROCESSES, /* number of LBAs */
					 read_full_complete, cb, 0);

	if (rc != 0) {
				fprintf(stderr, "starting write I/O failed\n");
				exit(1);
	}

	uint32_t target_core = it->second->internal_id / spdk_env_get_core_count(); //compute the core where the event should run in order that only 1 thread accesses the NVME QUEUE

	struct spdk_event * e = spdk_event_allocate(target_core,verify_event<std::unique_ptr<std::vector<std::unique_ptr<DiskBlock>>>>,cb,NULL);
	spdk_event_call(e);
}

std::future<std::unique_ptr<std::vector<std::unique_ptr<DiskBlock>>>> read_full(std::string disk,int k){
  std::map<std::string,std::unique_ptr<NVME_NAMESPACE>>::iterator it;

  it = namespaces.find(disk);
  if (it == namespaces.end()){
		throw std::invalid_argument( "Disk not found" );
  }

  int LBA_INDEX = k * NUM_PROCESSES;
  size_t BLOCK_SIZE = (it->second->lbaf + it->second->metadata_size);

  byte * buffer = (byte *) spdk_zmalloc(BLOCK_SIZE * NUM_PROCESSES, 0x1000, NULL, SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);

	if (buffer == NULL) {
		throw std::bad_alloc();
	}

  CallBack<std::unique_ptr<std::vector<std::unique_ptr<DiskBlock>>>> * cb = new CallBack<std::unique_ptr<std::vector<std::unique_ptr<DiskBlock>>>>(buffer,disk,LBA_INDEX,BLOCK_SIZE);
	cb->callback = std::promise< std::unique_ptr<std::vector<std::unique_ptr<DiskBlock>>> >();
  auto future = cb->callback.get_future();

	uint32_t target_core = it->second->internal_id / spdk_env_get_core_count();

	struct spdk_event * e = spdk_event_allocate(target_core,read_full_event,cb,NULL);
	spdk_event_call(e);
  //delete cb;

	return future;
}
