extern "C" {
	#include "spdk/event.h"
	#include "spdk/stdinc.h"
	#include "spdk/nvme.h"
	#include "spdk/vmd.h"
	#include "spdk/nvme_zns.h"
	#include "spdk/env.h"
	#include "spdk/thread.h"
}

#include "Disk/DiskPaxos.hpp"
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
#include <algorithm>

using namespace std;

typedef unsigned char byte;
static int NUM_PROCESSES = 0;
static int NUM_CONCENSOS_LANES = 0;

/**
Struct used to keep track of all controllers found.
*/

struct NVME_CONTROLER_V2 {
  struct spdk_nvme_ctrlr *ctrlr;
  string name;

  NVME_CONTROLER_V2(struct spdk_nvme_ctrlr * ctrlr,char * chr_name){
    this->ctrlr = ctrlr;
    name = string(chr_name);
  };
  ~NVME_CONTROLER_V2(){
    cout << "Exiting CTRLR: " << name << endl;
  };
};

/**
Struct to save some info about the nvme namespace used
*/

struct NVME_NAMESPACE_INFO {
  uint32_t lbaf; // size in bytes of each block
	uint32_t metadata_size; // bytes used by metadata of each block

	NVME_NAMESPACE_INFO(uint32_t lbaf,uint32_t metadata_size): lbaf(lbaf), metadata_size(metadata_size){};
	~NVME_NAMESPACE_INFO(){};
};

/**
Struct used to keep track of the namespace used in each controller.
*/

struct NVME_NAMESPACE_MULTITHREAD {
  struct spdk_nvme_ctrlr	*ctrlr;
	struct spdk_nvme_ns	*ns;
	map<uint32_t,struct spdk_nvme_qpair	*> qpairs;
	NVME_NAMESPACE_INFO info;

  NVME_NAMESPACE_MULTITHREAD(
    struct spdk_nvme_ctrlr	*ctrlr,
    struct spdk_nvme_ns	*ns
  ): ctrlr(ctrlr), ns(ns), info(NVME_NAMESPACE_INFO( spdk_nvme_ns_get_sector_size(ns), spdk_nvme_ns_get_md_size(ns))) {};
  ~NVME_NAMESPACE_MULTITHREAD(){
    cout << "Exiting NAMESPACE: " << endl;
  };
};

struct DiskOperation {
	string disk_id;
	int tick;
	size_t size_elem;
	byte * buffer;
	int status;
	uint32_t target_core;
	DiskPaxos * dp;

	DiskOperation(string d_id,int t,DiskPaxos * pointer,uint32_t target_core): disk_id(d_id), tick(t){
		status = 0;
		dp = pointer;
		this->target_core = target_core;
	};
	DiskOperation(string d_id,int t,size_t size_per_elem,DiskPaxos * pointer,uint32_t target_core): disk_id(d_id), tick(t), size_elem(size_per_elem){
		status = 0;
		dp = pointer;
		this->target_core = target_core;
	};
	~DiskOperation(){};
};

static set<string> addresses; //set a string identifying every disk
static vector<unique_ptr<NVME_CONTROLER_V2>> controllers;
static map<string,unique_ptr<NVME_NAMESPACE_MULTITHREAD>> namespaces;
static atomic<bool> ready (false);
static thread internal_spdk_event_launcher;

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

  NVME_NAMESPACE_MULTITHREAD * my_ns = new NVME_NAMESPACE_MULTITHREAD(ctrlr,ns);

	uint32_t n_cores = spdk_env_get_core_count();
	uint32_t k = spdk_env_get_first_core();
	for (uint32_t i = 0; i < n_cores; i++) {
		my_ns->qpairs.insert(pair<uint32_t,struct spdk_nvme_qpair	*>(k,spdk_nvme_ctrlr_alloc_io_qpair(ctrlr, NULL, 0)));
		k = spdk_env_get_next_core(k);
	}

  std::string addr(trid->traddr);
  namespaces.insert(pair<string,unique_ptr<NVME_NAMESPACE_MULTITHREAD>>(addr,unique_ptr<NVME_NAMESPACE_MULTITHREAD>(my_ns)));

  printf("  Namespace ID: %d size: %juGB %lu \n", spdk_nvme_ns_get_id(ns),
         spdk_nvme_ns_get_size(ns) / 1000000000, spdk_nvme_ns_get_num_sectors(ns));

  return 0;
}
static void attach_cb( void *cb_ctx, const struct spdk_nvme_transport_id *trid, struct spdk_nvme_ctrlr *ctrlr, const struct spdk_nvme_ctrlr_opts *opts){
  struct spdk_nvme_ns * ns;
	const struct spdk_nvme_ctrlr_data *cdata;

  //getting controller data
  cdata = spdk_nvme_ctrlr_get_data(ctrlr);
  char * chr_name = new char[1024];

  snprintf(chr_name, sizeof(char)*1024, "%-20.20s (%-20.20s)", cdata->mn, cdata->sn);

  NVME_CONTROLER_V2 * current_ctrlr = new NVME_CONTROLER_V2(ctrlr,chr_name);

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

  controllers.push_back(unique_ptr<NVME_CONTROLER_V2>(current_ctrlr));
}
static void cleanup(void){
  struct spdk_nvme_detach_ctx *detach_ctx = NULL;

  for(const auto& ns_entry : namespaces){
    const std::unique_ptr<NVME_NAMESPACE_MULTITHREAD>& ns = ns_entry.second;

		for (const auto& any : ns->qpairs) {
		   struct spdk_nvme_qpair	* qpair = any.second;
		   spdk_nvme_ctrlr_free_io_qpair(qpair);
		}
  }

  for(auto& ctrl : controllers){
    spdk_nvme_detach_async(ctrl->ctrlr, &detach_ctx);
  }

  while (detach_ctx && spdk_nvme_detach_poll_async(detach_ctx) == -EAGAIN){
		;
	}
}

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

	cout << "Succeded: " << spdk_env_get_core_count() << endl;
}
static void run_spdk_event_framework(){
  struct spdk_app_opts app_opts = {};
  spdk_app_opts_init(&app_opts, sizeof(app_opts));
	app_opts.name = "event_test";
	app_opts.reactor_mask = "0x2f";

  int rc = spdk_app_start(&app_opts, app_init, NULL);

	if (rc){
		cout << "Error might have occured" << endl;
	}

	cleanup();
	//spdk_vmd_fini();

	spdk_app_fini();
}

int spdk_start(int n_p,int n_k) {

	NUM_PROCESSES = n_p;
	NUM_CONCENSOS_LANES = n_k;

	internal_spdk_event_launcher = thread(run_spdk_event_framework);

	while(!ready);

  return 0;
}

void spdk_end() {
	spdk_app_stop(0);
	internal_spdk_event_launcher.join();
}

DiskPaxos::DiskPaxos(string input, int slot, uint32_t target_core, int pid){
  this->input = input;
  this->tick = 0;
  this->phase = 0;
  this->status = 0;
  this->slot = slot;
  this->pid = pid;
	this->nextBallot = pid;
	this->n_events = 0;

  DiskBlock * db = new DiskBlock();
	db->slot = slot;
  this->local_block = std::unique_ptr<DiskBlock>(db);

  this->target_core = target_core;
}

static void spawn_disk_paxos(void * arg1,void * arg2){
	DiskPaxos * dp = (DiskPaxos *) arg1;

	dp->startBallot();
}

void start_DiskPaxos(DiskPaxos * dp){
	struct spdk_event * e = spdk_event_allocate(dp->target_core,spawn_disk_paxos,dp,NULL);
	spdk_event_call(e);
}

void DiskPaxos::initPhase(){
	this->disksSeen.clear();
	this->blocksSeen.clear();

	DiskBlock * db = this->local_block->copy();
	this->blocksSeen.push_back(unique_ptr<DiskBlock>(db));
}

void DiskPaxos::startBallot(){
	cout << "Started a new Ballot" << endl;
	this->tick++;
	this->phase = 1;
	this->nextBallot += NUM_PROCESSES;
	this->local_block->mbal = this->nextBallot;
	this->initPhase();
	this->ReadAndWrite();
}

void DiskPaxos::phase2(){
	cout << "Began phase " << this->phase << " N_E: " << this->n_events << endl;
	this->phase = 2;
	this->initPhase();
	this->ReadAndWrite();
}

void DiskPaxos::endPhase(){
	this->tick++;
	cout << "Completed phase " << this->phase << " N_E: " << this->n_events << endl;

	for(auto & bk : this->blocksSeen)
		cout << bk->toString() << endl;

	if (this->phase == 1){
		this->local_block->bal = this->local_block->mbal;
		auto new_end = remove_if(this->blocksSeen.begin(),this->blocksSeen.end(),
			[](const unique_ptr<DiskBlock>& bk) {
				return bk->input.size() == 0;
			});
		this->blocksSeen.erase(new_end,this->blocksSeen.end());

		if (this->blocksSeen.size() != 0){
			cout << "Phase " << this->phase << " More than 1 block" << endl;
			auto max_blk = max_element(this->blocksSeen.begin(),this->blocksSeen.end(),
				[] (const unique_ptr<DiskBlock>& bk1, const unique_ptr<DiskBlock>& bk2) {
					return bk1->bal < bk2->bal;
				});
			this->local_block->input = (*max_blk)->input;
			cout << "Phase " << this->phase << " Choose: " << (*max_blk)->input << endl;
		}
		else{
			this->local_block->input = this->input;
		}
		this->phase2();
	}
	else{
		this->status = 1;
		cout << "Consensus archived: " << this->local_block->input << endl;
	}

}

static void verify_event(void * arg1, void * arg2){
	DiskOperation * dO = (DiskOperation *) arg1;

	map<string,unique_ptr<NVME_NAMESPACE_MULTITHREAD>>::iterator it;
	it = namespaces.find(dO->disk_id);

	if (!dO->status){
		map<uint32_t,struct spdk_nvme_qpair	*>::iterator it_qpair;
		it_qpair = it->second->qpairs.find(dO->target_core);

		spdk_nvme_qpair_process_completions(it_qpair->second, 0);

		struct spdk_event * e = spdk_event_allocate(dO->target_core,verify_event,dO,NULL);
		spdk_event_call(e);
	}
	else{
		delete dO;
	}
}

static void read_complete(void *arg,const struct spdk_nvme_cpl *completion){
	DiskOperation * dO = (DiskOperation *) arg;

	if (spdk_nvme_cpl_is_error(completion)) {
		fprintf(stderr, "I/O error status: %s\n", spdk_nvme_cpl_get_status_string(&completion->status));
		fprintf(stderr, "Write I/O failed, aborting run\n");
		dO->status = 2;
		exit(1);
	}

	DiskPaxos * dp = dO->dp;

	dp->n_events--;
	if (dO->tick == dp->tick){
		int size_elem = dO->size_elem;
		for (int i = 0; i < NUM_PROCESSES; i++) {
			if (i == dp->pid)
				continue;

			string db_serialized = bytes_to_string(dO->buffer + size_elem * i); //buffer to string
			DiskBlock * db = new DiskBlock();
			db->deserialize(db_serialized); //inverse of serialize

			if (db->isValid()){ // Verify if data read is valid
				if (db->slot <= dp->local_block->slot){
					if (db->slot == dp->local_block->slot) {
						dp->blocksSeen.push_back(unique_ptr<DiskBlock>(db));

						if (db->mbal > dp->local_block->mbal){
							dp->Abort(db->mbal);
							break;
						}
					}
					else{
						delete db;
					}
				}
				else {
					delete db;
					dp->Cancel();
					break;
				}
			}
			else{
				delete db;
			}
		}

		//se a operação for cancelada ou abortada o tick vai aumentar
		if (dO->tick == dp->tick){
			dp->disksSeen.insert(dO->disk_id);
			if (dp->disksSeen.size() > (addresses.size() / 2) ){
				dp->endPhase();
			}
		}
	}

	spdk_free(dO->buffer);
	dO->status = 1;
}

static void read_full_line(string disk_id,int tick,DiskPaxos * dp){
	map<string,unique_ptr<NVME_NAMESPACE_MULTITHREAD>>::iterator it;
	map<uint32_t,struct spdk_nvme_qpair	*>::iterator it_qpair;

	it = namespaces.find(disk_id); //get the namespace for the current disk
	it_qpair = it->second->qpairs.find(dp->target_core); // get the queue for the current thread
	int LBA_INDEX = (dp->slot % NUM_CONCENSOS_LANES) * NUM_PROCESSES; // index of the block to start reading from
	size_t BUFFER_SIZE = (it->second->info.lbaf + it->second->info.metadata_size);

	DiskOperation * dO = new DiskOperation(disk_id,tick,BUFFER_SIZE,dp,dp->target_core); // read data object
	dO->buffer = (byte *) spdk_zmalloc(BUFFER_SIZE * NUM_PROCESSES, 0x1000, NULL, SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);

	int rc = spdk_nvme_ns_cmd_read(it->second->ns, it_qpair->second , dO->buffer,
						LBA_INDEX, /* LBA start */
						NUM_PROCESSES, /* number of LBAs */
						read_complete, dO, 0); //submit a write operation to NVME

	if (rc != 0) {
			fprintf(stderr, "starting write I/O failed\n");
			exit(1);
	}

	dO->dp->n_events++;
	struct spdk_event * e = spdk_event_allocate(dp->target_core,verify_event,dO,NULL);
	spdk_event_call(e);
}

static void write_complete(void *arg,const struct spdk_nvme_cpl *completion){
	DiskOperation * dO = (DiskOperation *) arg;

	if (spdk_nvme_cpl_is_error(completion)) {
		fprintf(stderr, "I/O error status: %s\n", spdk_nvme_cpl_get_status_string(&completion->status));
		fprintf(stderr, "Write I/O failed, aborting run\n");
		dO->status = 2;
		exit(1);
	}

	spdk_free(dO->buffer);
	cout << "Write on " << dO->disk_id << " completed" << endl;
	if (dO->tick == dO->dp->tick){
		read_full_line(dO->disk_id,dO->tick,dO->dp);
	}
	dO->dp->n_events--;
	dO->status = 1;
}

void DiskPaxos::ReadAndWrite(){
	cout << "Started ReadAndWrite Phase: " << this->phase << endl;
	string local_db_serialized = this->local_block->serialize();
	map<string,unique_ptr<NVME_NAMESPACE_MULTITHREAD>>::iterator it;
	int LBA_INDEX = (this->slot % NUM_CONCENSOS_LANES) * NUM_PROCESSES + this->pid;

	map<uint32_t,struct spdk_nvme_qpair	*>::iterator it_qpair;

	for(auto disk_id : addresses){
	  it = namespaces.find(disk_id);
		if (it != namespaces.end()){

			size_t BUFFER_SIZE = (it->second->info.lbaf + it->second->info.metadata_size);
			DiskOperation * dO = new DiskOperation(disk_id,this->tick,this,this->target_core);
			dO->buffer = (byte *) spdk_zmalloc(BUFFER_SIZE, 0x1000, NULL, SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);
			string_to_bytes(local_db_serialized,dO->buffer);

			it_qpair = it->second->qpairs.find(this->target_core);

			int rc = spdk_nvme_ns_cmd_write(it->second->ns, it_qpair->second , dO->buffer,
								LBA_INDEX, /* LBA start */
								1, /* number of LBAs */
								write_complete, dO, SPDK_NVME_IO_FLAGS_FORCE_UNIT_ACCESS); //submit a write operation to NVME

			if (rc != 0) {
					fprintf(stderr, "starting write I/O failed\n");
					exit(1);
			}

			this->n_events++;
			struct spdk_event * e = spdk_event_allocate(this->target_core,verify_event,dO,NULL);
			spdk_event_call(e);
		}
	}
}

static void cleanup(void * arg1, void * arg2){
	DiskPaxos * dp = (DiskPaxos *) arg1;

	if (dp->n_events > 0){
		//there are still events running
		//spawn event recurvely
		struct spdk_event * e = spdk_event_allocate(dp->target_core,cleanup,dp,NULL);
		spdk_event_call(e);
	}
	else{
		dp->status = 2;
		cout << "Consensus cancelled" << endl;
	}
}

void DiskPaxos::Cancel(){
	this->tick++;
	//this->status = 2;

	//spawn event de cleanup
	struct spdk_event * e = spdk_event_allocate(this->target_core,cleanup,this,NULL);
	spdk_event_call(e);
}

void DiskPaxos::Abort(int mbal){
	int pid_responsible = mbal % NUM_PROCESSES;
	cout << "Abort" << endl;
	if (pid_responsible < this->pid){ // ainda posso ser leader
		cout << "Still running for leadership" << endl;
		this->startBallot();
	}
	else{ // tenho a certeza que não vou ser lider
		cout << "No longer running for leadership" << endl;
		this->Cancel();
	}
}
