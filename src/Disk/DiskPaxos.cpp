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
#include <future>
#include <chrono>
#include "Disk/SPDK_ENV.hpp"

using namespace std;

#define PROPOSAL_OFFSET 5000000

static void read_decision_cleanup(void * arg1, void * arg2);
static void read_decision_completion(void *arg,const struct spdk_nvme_cpl *completion);
static void read_multiple_decision_cleanup(void * arg1, void * arg2);

struct DiskOperation {
	string disk_id; //disk id
	int tick; //
	size_t size_elem;
	DiskPaxos::byte * buffer;
	int status;
	uint32_t target_core;
	DiskPaxos::DiskPaxos * dp;

	DiskOperation(string d_id,int t,DiskPaxos::DiskPaxos * pointer,uint32_t target_core): disk_id(d_id), tick(t){
		status = 0;
		dp = pointer;
		this->target_core = target_core;
	};

	/**
	Constructor used to read a line of blocks
	*/

	DiskOperation(string d_id,int t,size_t size_per_elem,DiskPaxos::DiskPaxos * pointer,uint32_t target_core): disk_id(d_id), tick(t), size_elem(size_per_elem){
		status = 0;
		dp = pointer;
		this->target_core = target_core;
	};
	DiskOperation(string d_id,uint32_t target_core): disk_id(d_id){
		status = 0;
		dp = NULL;
		this->target_core = target_core;
	};
	~DiskOperation(){};
};

struct Proposal {
	int pid;
	int slot;
	string command;
	uint32_t target_core;

	Proposal(int pid,int slot, string command, uint32_t target_core): pid(pid), slot(slot), command(command), target_core(target_core) {};
	~Proposal(){};
};
/**
struct MultiProposal {
	int pid;
	int starting_slot;
	vector<string> commands;
	uint32_t target_core;

	MultiProposal(int pid,int slot, vector<string> s_command, uint32_t target_core): pid(pid), starting_slot(slot), target_core(target_core) {
		commands = s_command;
	};
	~MultiProposal(){};
};*/

struct MultipleDecisionRead {
	int starting_slot;
	int amount;
	uint32_t target_core;
	int status;
	map<int,DiskBlock> decisions;
	promise< unique_ptr<map<int,DiskBlock>> > callback;
	set<string> disksSeen;
	int n_events;

	MultipleDecisionRead(int start, int size, uint32_t core){
		this->starting_slot = start;
		this->amount = size;
		this->status = 0;
		this->target_core = core;
		this->n_events = 0;
	};

	bool should_deliver(){
		if (this->disksSeen.size() > (SPDK_ENV::addresses.size() / 2))
			return true;
		return false;
	};

	void deliver(){
		this->status = 1;
		auto res = new map<int,DiskBlock>(this->decisions);
		this->callback.set_value(unique_ptr<map<int,DiskBlock>>(res));

		struct spdk_event * e = spdk_event_allocate(this->target_core,read_multiple_decision_cleanup,this,NULL);
		spdk_event_call(e);
	}

	~MultipleDecisionRead(){};
};

struct MultipleDecisionReadOpt {
	string disk_id; //identifier of a disk
	size_t size_elem; //block size supported
	int status; //current status, 0 running, 1 finished
	uint32_t target_core; // allocated core
	DiskPaxos::byte * buffer; //buffer used for the opt
	MultipleDecisionRead * dr; //master object for read

	MultipleDecisionReadOpt(
		string disk_id,
		size_t size_e,
		uint32_t target_core,
		MultipleDecisionRead * dr_p
	): disk_id(disk_id), size_elem(size_e),target_core(target_core), dr(dr_p){
		this->status = 0;
	};
	~MultipleDecisionReadOpt(){};
};

struct DecisionRead {
	int target_slot;
	uint32_t target_core;
	int status; //0 running, 1 ended;
	int n_events;
	promise<DiskBlock> callback;

	DecisionRead(int slot,int target_core): target_slot(slot), target_core(target_core), status(0),n_events(0){};
	~DecisionRead(){
		//std::cout << "DecisionRead cleaned" << std::endl;
	};
};

struct DecisionReadOpt {
	string disk_id; //identifier of a disk
	size_t size_elem; //block size supported
	int status; //current status, 0 running, 1 finished
	uint32_t target_core; // allocated core
	DiskPaxos::byte * buffer; //buffer used for the opt
	DecisionRead * dr; //master object for read

	DecisionReadOpt(
		string disk_id,
		size_t size_e,
		uint32_t target_core,
		DecisionRead * dr_p
	): disk_id(disk_id), size_elem(size_e),target_core(target_core), dr(dr_p){
		this->status = 0;
	};
	~DecisionReadOpt(){};
};

/**
Master Object to perform a read on a set of proposals
*/

struct LeaderRead {
	map<int,DiskBlock> proposals; //current map of each proposal found
	int starting_slot; //current value for K in the leader
	int number_of_slots; //number of K to check in advance
	set<string> disksSeen;
	int n_events;
	int status; //0 running, 1 ended;
	promise< unique_ptr<map<int,DiskBlock >> > callback;
	uint32_t target_core;
	int l_pid;

	LeaderRead(
		int slot,
		int n_slots,
		uint32_t target_core
	): starting_slot(slot), number_of_slots(n_slots), target_core(target_core){
		this->n_events = 0;
		this->status = 0;
		this->l_pid = -1;
	};
	LeaderRead(
		int slot,
		int n_slots,
		uint32_t target_core,
		int pid
	): starting_slot(slot), number_of_slots(n_slots), target_core(target_core), l_pid(pid){
		this->n_events = 0;
		this->status = 0;
	};

	~LeaderRead(){};
};

/**
Object to control a read from a single disk for a leader's read on replicas's proposals
*/

struct LeaderReadOpt {
	string disk_id; //identifier of a disk
	size_t size_elem; //block size supported
	int status; //current status, 0 running, 1 finished
	uint32_t target_core; // allocated core
	DiskPaxos::byte * buffer; //buffer used for the opt
	LeaderRead * ld; //master object for read

	LeaderReadOpt(
		string disk_id,
		size_t size_e,
		uint32_t target_core,
		LeaderRead * ld_p
	): disk_id(disk_id), size_elem(size_e),target_core(target_core), ld(ld_p){
		this->status = 0;
	};
	~LeaderReadOpt(){};
};
/**
struct SGL_Proposal {
	string disk_id; //identifier of a disk
	int size_elem; //block size supported
	int status; //current status, 0 running, 1 finished
	uint32_t target_core; // allocated core
	spdk_nvme_sgl_descriptor * sgl_segment;
	DiskPaxos::byte * buffer; //buffer used for the opt
	int current_offset;
	int current_segment;
	uint32_t segment_sizes [3];

	SGL_Proposal(
		string disk_id,
		size_t size_e,
		uint32_t target_core,
		DiskPaxos::byte * buff
	): disk_id(disk_id), size_elem(size_e),target_core(target_core){
		this->status = 0;
		this->buffer = buff;
		this->current_offset = 0;
		this->current_segment = 0;
	};

	void build_sgl_segment(
		int n_writes
	){

		int n_descriptors = 2 + (n_writes - 2) * 2 + 2 + 1;
		sgl_segment = (spdk_nvme_sgl_descriptor *) malloc(sizeof(struct spdk_nvme_sgl_descriptor) * n_descriptors);

		sgl_segment->address = (uint64_t) this->buffer;
		sgl_segment->unkeyed.length =  this->size_elem;
		sgl_segment->unkeyed.subtype =  SPDK_NVME_SGL_SUBTYPE_ADDRESS;
		sgl_segment->unkeyed.type =  SPDK_NVME_SGL_TYPE_DATA_BLOCK;

		(sgl_segment+1)->address = (uint64_t) (sgl_segment+2);
		(sgl_segment+1)->unkeyed.length =  (n_writes - 2)*32 + 32;
		(sgl_segment+1)->unkeyed.subtype =  SPDK_NVME_SGL_SUBTYPE_ADDRESS;
		(sgl_segment+1)->unkeyed.type =  SPDK_NVME_SGL_TYPE_SEGMENT;

		int n = 0;
		for (n = 0; n < (n_writes-2); n++) {
			(sgl_segment+2 + 2*n)->unkeyed.length =  this->size_elem * (SPDK_ENV::NUM_PROCESSES - 1);
			(sgl_segment+2 + 2*n)->unkeyed.type =  SPDK_NVME_SGL_TYPE_BIT_BUCKET;

			(sgl_segment+2 + 2*n + 1)->address = ((uint64_t) this->buffer) + ((n+1) * this->size_elem);
			(sgl_segment+2 + 2*n + 1)->unkeyed.length =  this->size_elem;
			(sgl_segment+2 + 2*n + 1)->unkeyed.subtype =  SPDK_NVME_SGL_SUBTYPE_ADDRESS;
			(sgl_segment+2 + 2*n + 1)->unkeyed.type =  SPDK_NVME_SGL_TYPE_DATA_BLOCK;
		}

		int index = 2 + 2*n;
		(sgl_segment+index)->unkeyed.length =  this->size_elem * (SPDK_ENV::NUM_PROCESSES - 1);
		(sgl_segment+index)->unkeyed.type =  SPDK_NVME_SGL_TYPE_BIT_BUCKET;

		index++;
		(sgl_segment+index)->address = (uint64_t) (sgl_segment+index+1);
		(sgl_segment+index)->unkeyed.length =  16;
		(sgl_segment+index)->unkeyed.subtype =  SPDK_NVME_SGL_SUBTYPE_ADDRESS;
		(sgl_segment+index)->unkeyed.type =  SPDK_NVME_SGL_TYPE_LAST_SEGMENT;

		index++;
		(sgl_segment+index)->address = ((uint64_t) this->buffer) + ((n+1) * this->size_elem);
		(sgl_segment+index)->unkeyed.length =  this->size_elem;
		(sgl_segment+index)->unkeyed.subtype =  SPDK_NVME_SGL_SUBTYPE_ADDRESS;
		(sgl_segment+index)->unkeyed.type =  SPDK_NVME_SGL_TYPE_DATA_BLOCK;


		for (int i = 0; i < n_descriptors; i++) {
			spdk_nvme_sgl_descriptor desc = *(sgl_segment+i);
			switch (desc.unkeyed.type) {
				case SPDK_NVME_SGL_TYPE_DATA_BLOCK:
					std::cout << "Type: SPDK_NVME_SGL_TYPE_DATA_BLOCK length: " << desc.unkeyed.length << " address: " << desc.address  << '\n';
					break;
				case SPDK_NVME_SGL_TYPE_BIT_BUCKET:
					std::cout << "Type: SPDK_NVME_SGL_TYPE_BIT_BUCKET length: " << desc.unkeyed.length << '\n';
					break;
				case SPDK_NVME_SGL_TYPE_LAST_SEGMENT:
					std::cout << "Type: SPDK_NVME_SGL_TYPE_LAST_SEGMENT length: " << desc.unkeyed.length << '\n';
					break;
				case SPDK_NVME_SGL_TYPE_SEGMENT:
					std::cout << "Type: SPDK_NVME_SGL_TYPE_SEGMENT length: " << desc.unkeyed.length << '\n';
					break;
				default:
					std::cout << "no match" << std::endl;
			}
		}

		segment_sizes[0] = 32;
		segment_sizes[1] = (n_writes - 2)*32 + 32;;
		segment_sizes[2] = 16;

		sgl_segment = (spdk_nvme_sgl_descriptor *) malloc(sizeof(struct spdk_nvme_sgl_descriptor) * 2);
		sgl_segment->address = this->buffer;
		sgl_segment->generic.subtype =  SPDK_NVME_SGL_SUBTYPE_ADDRESS;
		sgl_segment->generic.type =  SPDK_NVME_SGL_TYPE_DATA_BLOCK;
	};
};

static int nvme_req_next_sge_cb(void * cb_arg, void ** address,uint32_t *length){
	SGL_Proposal * sgl = (SGL_Proposal *) cb_arg;
	std::cout << "nvme_req_next_sge_cb called, offset: " << sgl->current_offset << '\n';

	//length = sgl->segment_sizes[sgl->current_segment];
	//address = ((void *) sgl->sgl_segment) + sgl->current_offset;
	*length = 4096;
	*address = ((void *) sgl->sgl_segment);
	sgl->current_segment++;

	return 0;
}

static void nvme_req_reset_sge_cb(void *cb_arg, uint32_t offset){
	std::cout << "nvme_req_reset_sge_cb called, offset: " << offset << '\n';
	SGL_Proposal * sgl = (SGL_Proposal *) cb_arg;
	sgl->current_offset += offset;
}
*/
/**
Function to write a encode byte block into a buffer;
the first 4 bytes keep the size of the block.
*/

static void string_to_bytes(std::string str, DiskPaxos::byte * buffer){
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

static std::string bytes_to_string(DiskPaxos::byte * buffer){
	int size = int((unsigned char)(buffer[0]) |
            (unsigned char)(buffer[1]) << 8 |
            (unsigned char)(buffer[2]) << 16 |
            (unsigned char)(buffer[3]) << 24);

	if (size > 1024){
		return std::string((char *)buffer,0);
	}

	return std::string((char *)buffer+4,size);
}

DiskPaxos::DiskPaxos::DiskPaxos(string input, int slot, int pid){
  this->input = input;
  this->tick = 0;
  this->phase = 0;
  this->status = 0;
  this->slot = slot;
  this->pid = pid;
	this->nextBallot = pid;
	this->n_events = 0;
	this->finished = 0;

  DiskBlock * db = new DiskBlock();
	db->slot = slot;
  this->local_block = std::unique_ptr<DiskBlock>(db);

  this->target_core = 0;
}

static void spawn_disk_paxos(void * arg1,void * arg2){
	DiskPaxos::DiskPaxos * dp = (DiskPaxos::DiskPaxos *) arg1;
	//cout << "Starting consensus for slot: " << dp->slot << " on core: " << dp->target_core << endl;
	dp->startBallot();
}

void DiskPaxos::DiskPaxos::initPhase(){
	this->disksSeen.clear();
	this->blocksSeen.clear();

	DiskBlock * db = this->local_block->copy();
	this->blocksSeen.push_back(unique_ptr<DiskBlock>(db));
}

void DiskPaxos::DiskPaxos::startBallot(){
	//cout << "Started a new Ballot" << endl;
	this->tick++;
	this->phase = 1;
	this->nextBallot += SPDK_ENV::NUM_PROCESSES;
	this->local_block->mbal = this->nextBallot;
	this->initPhase();
	this->ReadAndWrite();
}

void DiskPaxos::DiskPaxos::phase2(){
	//cout << "Began phase " << this->phase << " N_E: " << this->n_events << endl;
	this->phase = 2;
	this->initPhase();
	this->ReadAndWrite();
}

void DiskPaxos::DiskPaxos::endPhase(){
	this->tick++;
	//cout << "Completed phase " << this->phase << " N_E: " << this->n_events << endl;

	/** Printing of blocks
	for(auto & bk : this->blocksSeen)
		cout << bk->toString() << endl;*/

	if (this->phase == 1){
		this->local_block->bal = this->local_block->mbal;
		auto new_end = remove_if(this->blocksSeen.begin(),this->blocksSeen.end(),
			[](const unique_ptr<DiskBlock>& bk) {
				return bk->input.size() == 0;
			});
		this->blocksSeen.erase(new_end,this->blocksSeen.end());

		if (this->blocksSeen.size() != 0){
			//cout << "Phase " << this->phase << " More than 1 block" << endl;
			auto max_blk = max_element(this->blocksSeen.begin(),this->blocksSeen.end(),
				[] (const unique_ptr<DiskBlock>& bk1, const unique_ptr<DiskBlock>& bk2) {
					return bk1->bal < bk2->bal;
				});
			this->local_block->input = (*max_blk)->input;
			//cout << "Phase " << this->phase << " Choose: " << (*max_blk)->input << endl;
		}
		else{
			this->local_block->input = this->input;
		}
		this->phase2();
	}
	else{
		this->Commit();
	}
}

static void verify_event(void * arg1, void * arg2){
	DiskOperation * dO = (DiskOperation *) arg1;

	map<string,unique_ptr<SPDK_ENV::NVME_NAMESPACE_MULTITHREAD>>::iterator it;
	it = SPDK_ENV::namespaces.find(dO->disk_id);

	if (!dO->status){
		map<uint32_t,struct spdk_nvme_qpair	*>::iterator it_qpair;
		it_qpair = it->second->qpairs.find(dO->target_core);

		spdk_nvme_qpair_process_completions(it_qpair->second, 1);

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
		fprintf(stderr, "I/O error status: %s on read_complete\n", spdk_nvme_cpl_get_status_string(&completion->status));
		fprintf(stderr, "Write I/O failed, aborting run\n");
		dO->status = 2;
		exit(1);
	}

	DiskPaxos::DiskPaxos * dp = dO->dp;

	dp->n_events--;
	if (dO->tick == dp->tick){
		int size_elem = dO->size_elem;
		for (int i = 0; i < SPDK_ENV::NUM_PROCESSES; i++) {
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
					int mbal = db->mbal;
					delete db;
					dp->SkipLateLeader(mbal);
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
			if (dp->disksSeen.size() > (SPDK_ENV::addresses.size() / 2) ){
				dp->endPhase();
			}
		}
	}

	spdk_free(dO->buffer);
	dO->status = 1;
	SPDK_ENV::SCHEDULE_EVENTS[dp->target_core]--;
}

static void read_full_line(string disk_id,int tick,DiskPaxos::DiskPaxos * dp){
	map<string,unique_ptr<SPDK_ENV::NVME_NAMESPACE_MULTITHREAD>>::iterator it;
	map<uint32_t,struct spdk_nvme_qpair	*>::iterator it_qpair;

	it = SPDK_ENV::namespaces.find(disk_id); //get the namespace for the current disk
	it_qpair = it->second->qpairs.find(dp->target_core); // get the queue for the current thread
	int LBA_INDEX = (dp->slot % SPDK_ENV::NUM_CONCENSOS_LANES) * SPDK_ENV::NUM_PROCESSES; // index of the block to start reading from
	size_t BUFFER_SIZE = (it->second->info.lbaf + it->second->info.metadata_size);

	DiskOperation * dO = new DiskOperation(disk_id,tick,BUFFER_SIZE,dp,dp->target_core); // read data object
	dO->buffer = (DiskPaxos::byte *) spdk_zmalloc(BUFFER_SIZE * SPDK_ENV::NUM_PROCESSES, 0x1000, NULL, SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);

	if (dO->buffer == NULL)
		fprintf(stderr, "NULL Buffer on read_full_line\n");

	int rc = spdk_nvme_ns_cmd_read(it->second->ns, it_qpair->second , dO->buffer,
						LBA_INDEX, /* LBA start */
						SPDK_ENV::NUM_PROCESSES, /* number of LBAs */
						read_complete, dO, 0); //submit a write operation to NVME

	if (rc != 0) {
			SPDK_ENV::error_on_cmd_submit(rc,"read_full_line","read");
			exit(1);
	}

	SPDK_ENV::SCHEDULE_EVENTS[dp->target_core]++;
	dO->dp->n_events++;
	struct spdk_event * e = spdk_event_allocate(dp->target_core,verify_event,dO,NULL);
	spdk_event_call(e);
}

static void write_complete(void *arg,const struct spdk_nvme_cpl *completion){
	DiskOperation * dO = (DiskOperation *) arg;

	if (spdk_nvme_cpl_is_error(completion)) {
		fprintf(stderr, "I/O error status: %s on write_complete\n", spdk_nvme_cpl_get_status_string(&completion->status));
		fprintf(stderr, "Write I/O failed, aborting run\n");
		dO->status = 2;
		exit(1);
	}

	spdk_free(dO->buffer);
	//cout << "Write on " << dO->disk_id << " completed" << endl;
	if (dO->tick == dO->dp->tick){
		read_full_line(dO->disk_id,dO->tick,dO->dp);
	}
	dO->dp->n_events--;
	dO->status = 1;
	SPDK_ENV::SCHEDULE_EVENTS[dO->target_core]--;
}

void DiskPaxos::DiskPaxos::ReadAndWrite(){
	//cout << "Started ReadAndWrite Phase: " << this->phase << endl;
	string local_db_serialized = this->local_block->serialize();
	map<string,unique_ptr<SPDK_ENV::NVME_NAMESPACE_MULTITHREAD>>::iterator it;
	int LBA_INDEX = (this->slot % SPDK_ENV::NUM_CONCENSOS_LANES) * SPDK_ENV::NUM_PROCESSES + this->pid;

	map<uint32_t,struct spdk_nvme_qpair	*>::iterator it_qpair;

	for(auto disk_id : SPDK_ENV::addresses){
	  it = SPDK_ENV::namespaces.find(disk_id);
		if (it != SPDK_ENV::namespaces.end()){

			size_t BUFFER_SIZE = (it->second->info.lbaf + it->second->info.metadata_size);
			DiskOperation * dO = new DiskOperation(disk_id,this->tick,this,this->target_core);
			dO->buffer = (byte *) spdk_zmalloc(BUFFER_SIZE, 0x1000, NULL, SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);
			string_to_bytes(local_db_serialized,dO->buffer);

			it_qpair = it->second->qpairs.find(this->target_core);

			if (dO->buffer == NULL) //apagar depois
				fprintf(stderr, "NULL Buffer on ReadAndWrite\n");

			int rc = spdk_nvme_ns_cmd_write(it->second->ns, it_qpair->second , dO->buffer,
								LBA_INDEX, /* LBA start */
								1, /* number of LBAs */
								write_complete, dO, 0); //submit a write operation to NVME

			if (rc != 0) {
					SPDK_ENV::error_on_cmd_submit(rc,"ReadAndWrite","write");
					exit(1);
			}

			SPDK_ENV::SCHEDULE_EVENTS[this->target_core]++;
			this->n_events++;
			struct spdk_event * e = spdk_event_allocate(this->target_core,verify_event,dO,NULL);
			spdk_event_call(e);
		}
	}
}

static void cleanup(void * arg1, void * arg2){
	DiskPaxos::DiskPaxos * dp = (DiskPaxos::DiskPaxos *) arg1;

	if (dp->n_events > 0){
		//there are still events running
		//spawn event recurvely
		struct spdk_event * e = spdk_event_allocate(dp->target_core,cleanup,dp,NULL);
		spdk_event_call(e);
	}
	else{
		dp->finished = 1;
		//cout << "Consensus finished" << endl;
	}
}

void DiskPaxos::DiskPaxos::Cancel(){
	this->tick++;
	this->status = 2;

	//spawn event de cleanup
	this->finished = 2;
	struct spdk_event * e = spdk_event_allocate(this->target_core,cleanup,this,NULL);
	spdk_event_call(e);
}
/*
 	Function used went a newer slot is found.
 */
void DiskPaxos::DiskPaxos::SkipLateLeader(int mbal){
	this->tick++;

	int pid_responsible = mbal % SPDK_ENV::NUM_PROCESSES;

	if (pid_responsible < this->pid){
		this->status = 3;// Estou atrasado mas ainda posso ser líder.
	}
	else{ // tenho a certeza que não vou ser lider
		this->status = 2;
	}

	//spawn event de cleanup
	this->finished = 2;
	struct spdk_event * e = spdk_event_allocate(this->target_core,cleanup,this,NULL);
	spdk_event_call(e);
}

void DiskPaxos::DiskPaxos::Abort(int mbal){
	int pid_responsible = mbal % SPDK_ENV::NUM_PROCESSES;
	//cout << "Abort" << endl;
	if (pid_responsible < this->pid){ // ainda posso ser leader
		//cout << "Still running for leadership" << endl;
		this->startBallot();
	}
	else{ // tenho a certeza que não vou ser lider
		//cout << "No longer running for leadership" << endl;
		this->Cancel();
	}
}

static void write_commit_complete(void *arg,const struct spdk_nvme_cpl *completion){
	DiskOperation * dO = (DiskOperation *) arg;

	if (spdk_nvme_cpl_is_error(completion)) {
		fprintf(stderr, "I/O error status: %s write_commit_complete\n", spdk_nvme_cpl_get_status_string(&completion->status));
		fprintf(stderr, "Write I/O failed, aborting run\n");
		dO->status = 2;
		exit(1);
	}

	spdk_free(dO->buffer);
	//cout << "Commit on: " << dO->disk_id << " started" << endl;
	dO->dp->n_events--;
	dO->status = 1;

	DiskPaxos::DiskPaxos * dp = dO->dp;

	dp->disksSeen.insert(dO->disk_id);
	if (dp->disksSeen.size() > (SPDK_ENV::addresses.size() / 2) && dp->status == 0){
		dp->status = 1;
		dp->finished = 2;
		struct spdk_event * e = spdk_event_allocate(dp->target_core,cleanup,dp,NULL);
		spdk_event_call(e);
	}
	SPDK_ENV::SCHEDULE_EVENTS[dp->target_core]--;
}

void DiskPaxos::DiskPaxos::Commit(){
	//cout << "Consensus archived: " << this->local_block->input << " for slot: " << this->local_block->slot << " on core " << this->target_core << endl;

	string local_db_serialized = this->local_block->serialize();
	map<string,unique_ptr<SPDK_ENV::NVME_NAMESPACE_MULTITHREAD>>::iterator it;
	int LBA_INDEX = SPDK_ENV::NUM_CONCENSOS_LANES * SPDK_ENV::NUM_PROCESSES + this->slot;

	map<uint32_t,struct spdk_nvme_qpair	*>::iterator it_qpair;
	this->disksSeen.clear();

	for(auto disk_id: SPDK_ENV::addresses){
		it = SPDK_ENV::namespaces.find(disk_id);

		if (it != SPDK_ENV::namespaces.end()){

			size_t BUFFER_SIZE = (it->second->info.lbaf + it->second->info.metadata_size);
			DiskOperation * dO = new DiskOperation(disk_id,this->tick,this,this->target_core);
			dO->buffer = (byte *) spdk_zmalloc(BUFFER_SIZE, 0x1000, NULL, SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);
			string_to_bytes(local_db_serialized,dO->buffer);

			it_qpair = it->second->qpairs.find(this->target_core);

			if (dO->buffer == NULL) //apagar depois
				fprintf(stderr, "NULL Buffer on Commit\n");

			int rc = spdk_nvme_ns_cmd_write(it->second->ns, it_qpair->second , dO->buffer,
								LBA_INDEX, /* LBA start */
								1, /* number of LBAs */
								write_commit_complete, dO, 0); //submit a write operation to NVME

			if (rc != 0) {
				SPDK_ENV::error_on_cmd_submit(rc,"Commit","write");
				exit(1);
			}
			SPDK_ENV::SCHEDULE_EVENTS[this->target_core]++;
			this->n_events++;
			struct spdk_event * e = spdk_event_allocate(this->target_core,verify_event,dO,NULL);
			spdk_event_call(e);
		}
	}
}

static void simple_write_complete(void *arg,const struct spdk_nvme_cpl *completion){
	DiskOperation * dO = (DiskOperation *) arg;

	if (spdk_nvme_cpl_is_error(completion)) {
		fprintf(stderr, "I/O error status: %s  simple_write_complete\n", spdk_nvme_cpl_get_status_string(&completion->status));
		fprintf(stderr, "Write I/O failed, aborting run\n");
		dO->status = 2;
		exit(1);
	}

	spdk_free(dO->buffer);
	dO->status = 1;
	SPDK_ENV::SCHEDULE_EVENTS[dO->target_core]--;
}

static void internal_proposal(void * arg1, void * arg2){
	Proposal * props = (Proposal *) arg1;

	DiskBlock db;
	db.slot = props->slot;
	db.input = props->command;

	string local_db_serialized = db.serialize();

	map<string,unique_ptr<SPDK_ENV::NVME_NAMESPACE_MULTITHREAD>>::iterator it;
	map<uint32_t,struct spdk_nvme_qpair	*>::iterator it_qpair;
	int LBA_INDEX = SPDK_ENV::NUM_CONCENSOS_LANES * SPDK_ENV::NUM_PROCESSES + PROPOSAL_OFFSET + db.slot * SPDK_ENV::NUM_PROCESSES + props->pid;

	for(auto disk_id: SPDK_ENV::addresses){
		it = SPDK_ENV::namespaces.find(disk_id);

		if (it != SPDK_ENV::namespaces.end()){

			size_t BUFFER_SIZE = (it->second->info.lbaf + it->second->info.metadata_size);
			DiskOperation * dO = new DiskOperation(disk_id,props->target_core);
			dO->buffer = (DiskPaxos::byte *) spdk_zmalloc(BUFFER_SIZE, 0x1000, NULL, SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);
			string_to_bytes(local_db_serialized,dO->buffer);

			it_qpair = it->second->qpairs.find(props->target_core);

			if (dO->buffer == NULL) //apagar depois
				fprintf(stderr, "NULL Buffer on Proposal\n");

			int rc = spdk_nvme_ns_cmd_write(it->second->ns, it_qpair->second , dO->buffer,
								LBA_INDEX, /* LBA start */
								1, /* number of LBAs */
								simple_write_complete, dO, 0); //submit a write operation to NVME

			if (rc != 0) {
					SPDK_ENV::error_on_cmd_submit(rc,"Proposal","write");
					exit(1);
			}

			SPDK_ENV::SCHEDULE_EVENTS[dO->target_core]++;
			struct spdk_event * e = spdk_event_allocate(dO->target_core,verify_event,dO,NULL);
			spdk_event_call(e);
		}
	}

	delete props;
}

/**
static void test_func(void *arg,const struct spdk_nvme_cpl *completion){
	SGL_Proposal * dO = (SGL_Proposal *) arg;

	if (spdk_nvme_cpl_is_error(completion)) {
		fprintf(stderr, "I/O error status: %s  test_func\n", spdk_nvme_cpl_get_status_string(&completion->status));
		fprintf(stderr, "Write I/O failed, aborting run\n");
		dO->status = 2;
		exit(1);
	}

	string db_serialized = bytes_to_string(dO->buffer); //buffer to string
	DiskBlock * db = new DiskBlock();
	db->deserialize(db_serialized); //inverse of serialize

	spdk_free(dO->buffer);
	dO->status = 1;
	std::cout << "Test func success: " << db->toString() << '\n';
	delete db;
}

static void verify_event_test(void * arg1, void * arg2){
	SGL_Proposal * dO = (SGL_Proposal *) arg1;

	map<string,unique_ptr<SPDK_ENV::NVME_NAMESPACE_MULTITHREAD>>::iterator it;
	it = SPDK_ENV::namespaces.find(dO->disk_id);

	if (!dO->status){
		map<uint32_t,struct spdk_nvme_qpair	*>::iterator it_qpair;
		it_qpair = it->second->qpairs.find(dO->target_core);

		int r = spdk_nvme_qpair_process_completions(it_qpair->second, 0);
		if (r >= 0){
			struct spdk_event * e = spdk_event_allocate(dO->target_core,verify_event_test,dO,NULL);
			spdk_event_call(e);
		}
		else{
			std::cout << "error on completions" << '\n';
		}
	}
	else{
		delete dO;
	}
}

static void internal_multi_proposal(void * arg1, void * arg2){
	MultiProposal * props = (MultiProposal *) arg1;

	DiskBlock db;

	vector<string> dbs_serialized;

	int starting_slot = props->starting_slot;
	for(auto str : props->commands){
		db.slot = starting_slot;
		db.input = str;
		starting_slot++;
		dbs_serialized.push_back(db.serialize());
	}

	std::cout << dbs_serialized.size() << '\n';

	map<string,unique_ptr<SPDK_ENV::NVME_NAMESPACE_MULTITHREAD>>::iterator it;
	map<uint32_t,struct spdk_nvme_qpair	*>::iterator it_qpair;
	int LBA_INDEX = SPDK_ENV::NUM_CONCENSOS_LANES * SPDK_ENV::NUM_PROCESSES + PROPOSAL_OFFSET + props->starting_slot * SPDK_ENV::NUM_PROCESSES + props->pid;

	for(auto disk_id: SPDK_ENV::addresses){
		it = SPDK_ENV::namespaces.find(disk_id);

		if (it != SPDK_ENV::namespaces.end()){

			size_t BUFFER_SIZE = (it->second->info.lbaf + it->second->info.metadata_size);
			DiskPaxos::byte * buff = (DiskPaxos::byte *) spdk_zmalloc(BUFFER_SIZE * dbs_serialized.size(), 0x1000, NULL, SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);

			int index = 0;
			for(auto str : dbs_serialized){
				string_to_bytes(str,buff + index);
				index += BUFFER_SIZE;
			}

			SGL_Proposal * sgl_prop = new SGL_Proposal(disk_id,BUFFER_SIZE,props->target_core,buff);
			sgl_prop->build_sgl_segment(4);

			it_qpair = it->second->qpairs.find(props->target_core);

			if (buff == NULL) //apagar depois
				fprintf(stderr, "NULL Buffer on Proposal\n");

			int rc = spdk_nvme_ns_cmd_writev(it->second->ns, it_qpair->second ,
								LBA_INDEX, // LBA start
								//dbs_serialized.size() * SPDK_ENV::NUM_PROCESSES, // number of LBAs
								1,
								test_func,
								sgl_prop,
								0,
								nvme_req_reset_sge_cb,
								nvme_req_next_sge_cb
							); //submit a write operation to NVME

			if (rc != 0) {
					SPDK_ENV::error_on_cmd_submit(rc,"MultiProposal","write");
					exit(1);
			}

			struct spdk_event * e = spdk_event_allocate(sgl_prop->target_core,verify_event_test,sgl_prop,NULL);
			spdk_event_call(e);
		}
	}

	delete props;
}
*/

void DiskPaxos::propose(int pid, int slot,string command){
	uint32_t core = SPDK_ENV::allocate_replica_core();
	Proposal * p = new Proposal(pid,slot,command,core);

	struct spdk_event * e = spdk_event_allocate(core,internal_proposal,p,NULL);
	spdk_event_call(e);
}

/**
void DiskPaxos::propose_sgl(int pid, int starting_slot,vector<string> commands){
	uint32_t core = SPDK_ENV::allocate_replica_core();
	MultiProposal * p = new MultiProposal(pid,starting_slot,commands,core);

	struct spdk_event * e = spdk_event_allocate(core,internal_multi_proposal,p,NULL);
	spdk_event_call(e);
}
*/
void DiskPaxos::propose(int pid, int slot,string command,uint32_t target_core){
	uint32_t core = target_core;
	Proposal * p = new Proposal(pid,slot,command,core);

	struct spdk_event * e = spdk_event_allocate(core,internal_proposal,p,NULL);
	spdk_event_call(e);
}

static void verify_event_leader(void * arg1, void * arg2){
	LeaderReadOpt * dO = (LeaderReadOpt *) arg1;

	map<string,unique_ptr<SPDK_ENV::NVME_NAMESPACE_MULTITHREAD>>::iterator it;
	it = SPDK_ENV::namespaces.find(dO->disk_id);

	if (!dO->status){
		map<uint32_t,struct spdk_nvme_qpair	*>::iterator it_qpair;
		it_qpair = it->second->qpairs.find(dO->target_core);

		spdk_nvme_qpair_process_completions(it_qpair->second, 1);

		struct spdk_event * e = spdk_event_allocate(dO->target_core,verify_event_leader,dO,NULL);
		spdk_event_call(e);
	}
	else{
		delete dO;
	}
}

static void LeaderRead_cleanup(void * arg1, void * arg2){
	LeaderRead * ld = (LeaderRead * ) arg1;

	if (ld->n_events > 0){
		struct spdk_event * e = spdk_event_allocate(ld->target_core,LeaderRead_cleanup,ld,NULL);
		spdk_event_call(e);
	}
	else{
		delete ld;
	}
}

static void leader_read_completion(void *arg,const struct spdk_nvme_cpl *completion){
	LeaderReadOpt * ld_opt = (LeaderReadOpt *) arg;
	LeaderRead * ld = ld_opt->ld;

	if (spdk_nvme_cpl_is_error(completion)) {
		fprintf(stderr, "I/O error status: %s leader_read_completion on slot %d\n", spdk_nvme_cpl_get_status_string(&completion->status),ld->starting_slot);
		fprintf(stderr, "Read I/O failed, aborting run\n");

		std::cout << "CTRL STATUS: " << SPDK_ENV::ctrlr_current_status(ld_opt->disk_id) << '\n';
		SPDK_ENV::print_qpair_failure_reason(ld_opt->disk_id,ld_opt->target_core);
		ld_opt->status = 2;

		SPDK_ENV::qpair_reconnect_attempt(ld_opt->disk_id,ld_opt->target_core);

		//bool res = SPDK_ENV::reconnect(ld_opt->disk_id,ld_opt->target_core,10);
		//std::cout << "reconnect = " << res << '\n';
		//std::cout << "EVENT_COUNT: " << SPDK_ENV::SCHEDULE_EVENTS[ld_opt->target_core] << '\n';
		exit(-1);
	}
	SPDK_ENV::SCHEDULE_EVENTS[ld_opt->target_core]--;

	if (ld->status){ //already ended the read from a majority of disks
		ld->n_events--;
		spdk_free(ld_opt->buffer);
		ld_opt->status = 1;
		return;
	}

	map<int,DiskBlock>::iterator it;

	for(int i = 0; i < ld->number_of_slots; i++){
		it = ld->proposals.find(i + ld->starting_slot);
		if (it == ld->proposals.end()){ //if the current slot doesn't have a command
			if (ld->l_pid != -1){
				string db_serialized = bytes_to_string(ld_opt->buffer + ld_opt->size_elem * (i * SPDK_ENV::NUM_PROCESSES + ld->l_pid)); //buffer to string
				DiskBlock db;
				db.deserialize(db_serialized); //inverse of serialize

				if (db.isValid()){
					ld->proposals.insert(pair<int,DiskBlock>(i + ld->starting_slot,db));
					break;
				}
			}


			for (int j = 0; j < SPDK_ENV::NUM_PROCESSES; j++) {
				string db_serialized = bytes_to_string(ld_opt->buffer + ld_opt->size_elem * (i * SPDK_ENV::NUM_PROCESSES + j)); //buffer to string
				DiskBlock db;
				db.deserialize(db_serialized); //inverse of serialize

				if (db.isValid()){
					ld->proposals.insert(pair<int,DiskBlock>(i + ld->starting_slot,db));
					break;
				}
			}
		}
	}
	ld->disksSeen.insert(ld_opt->disk_id);


	if (ld->disksSeen.size() > (SPDK_ENV::addresses.size() / 2)){
		ld->status = 1;
		auto res = new map<int,DiskBlock>(ld->proposals);
		ld->callback.set_value(unique_ptr<map<int,DiskBlock>>(res));

		struct spdk_event * e = spdk_event_allocate(ld->target_core,LeaderRead_cleanup,ld,NULL);
		spdk_event_call(e);
	}
	ld->n_events--;

	spdk_free(ld_opt->buffer);
	ld_opt->status = 1;

	//std::cout << "Read Proposal Completed" << '\n';
}

static void read_list_proposals(void * arg1,void * arg2){
	LeaderRead * ld = (LeaderRead *) arg1;

	map<string,unique_ptr<SPDK_ENV::NVME_NAMESPACE_MULTITHREAD>>::iterator it;
	map<uint32_t,struct spdk_nvme_qpair	*>::iterator it_qpair;

	int LBA_INDEX = SPDK_ENV::NUM_CONCENSOS_LANES * SPDK_ENV::NUM_PROCESSES + PROPOSAL_OFFSET + ld->starting_slot * SPDK_ENV::NUM_PROCESSES;

	for(auto disk_id: SPDK_ENV::addresses){
		it = SPDK_ENV::namespaces.find(disk_id);

		/**
		using namespace std::chrono_literals;
		std::cout << "Sleeping" << std::endl;
		std::this_thread::sleep_for(5000ms);
		a = spdk_nvme_ctrlr_process_admin_completions(it->second->ctrlr);
		std::cout << "5sec a= " << a << std::endl;

		std::this_thread::sleep_for(5000ms);
		a = spdk_nvme_ctrlr_process_admin_completions(it->second->ctrlr);
		std::cout << "10sec a= " << a << std::endl;
		std::this_thread::sleep_for(5000ms);
		a = spdk_nvme_ctrlr_process_admin_completions(it->second->ctrlr);
		std::cout << "15sec a= " << a << std::endl;
		std::cout << "Wook up" << std::endl;
		*/

		if (it != SPDK_ENV::namespaces.end()){

			size_t BUFFER_SIZE = (it->second->info.lbaf + it->second->info.metadata_size);
			LeaderReadOpt * ld_opt = new LeaderReadOpt(disk_id,BUFFER_SIZE,ld->target_core,ld);

			ld_opt->buffer = (DiskPaxos::byte *) spdk_zmalloc(BUFFER_SIZE * ld->number_of_slots * SPDK_ENV::NUM_PROCESSES, 0x1000, NULL, SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);
			it_qpair = it->second->qpairs.find(ld_opt->target_core);

			if (ld_opt->buffer == NULL) //apagar depois
				fprintf(stderr, "NULL Buffer on read_list_proposals\n");

			int rc = spdk_nvme_ns_cmd_read(it->second->ns, it_qpair->second , ld_opt->buffer,
								LBA_INDEX,
								SPDK_ENV::NUM_PROCESSES*ld->number_of_slots,
								leader_read_completion, ld_opt,0);

			if (rc != 0) {
					SPDK_ENV::error_on_cmd_submit(rc,"read_list_proposals","read");
					exit(1);
			}

			SPDK_ENV::SCHEDULE_EVENTS[ld_opt->target_core]++;
			ld->n_events++;
			struct spdk_event * e = spdk_event_allocate(ld_opt->target_core,verify_event_leader,ld_opt,NULL);
			spdk_event_call(e);
		}
	}

}

std::future<unique_ptr<map<int,DiskBlock>> > DiskPaxos::read_proposals(int k,int number_of_slots){
	uint32_t core = SPDK_ENV::allocate_leader_core();
	LeaderRead * ld = new LeaderRead(k,number_of_slots,core);

	ld->callback = promise<unique_ptr< map<int,DiskBlock> >>();
	auto response = ld->callback.get_future();

	struct spdk_event * e = spdk_event_allocate(ld->target_core,read_list_proposals,ld,NULL);
	spdk_event_call(e);

	return response;
}

std::future<unique_ptr<map<int,DiskBlock>> > DiskPaxos::read_proposals(int k,int number_of_slots,int pid){
	uint32_t core = SPDK_ENV::allocate_leader_core();
	LeaderRead * ld = new LeaderRead(k,number_of_slots,core,pid);

	ld->callback = promise<unique_ptr< map<int,DiskBlock> >>();
	auto response = ld->callback.get_future();

	struct spdk_event * e = spdk_event_allocate(ld->target_core,read_list_proposals,ld,NULL);
	spdk_event_call(e);

	return response;
}

static void read_decision_repeat(DecisionReadOpt * dO){

	map<string,unique_ptr<SPDK_ENV::NVME_NAMESPACE_MULTITHREAD>>::iterator it;
	map<uint32_t,struct spdk_nvme_qpair	*>::iterator it_qpair;

	int LBA_INDEX = SPDK_ENV::NUM_CONCENSOS_LANES * SPDK_ENV::NUM_PROCESSES + dO->dr->target_slot;

	it = SPDK_ENV::namespaces.find(dO->disk_id);
	it_qpair = it->second->qpairs.find(dO->target_core);

	if (dO->buffer == NULL) //apagar depois
		fprintf(stderr, "NULL Buffer on read_decision_repeat\n");

	int rc = spdk_nvme_ns_cmd_read(it->second->ns, it_qpair->second , dO->buffer,
						LBA_INDEX,
						1,
						read_decision_completion, dO,0);
	SPDK_ENV::SCHEDULE_EVENTS[dO->target_core]++;

	if (rc != 0) {
			SPDK_ENV::error_on_cmd_submit(rc,"read_decision_repeat","read");
			exit(1);
	}
}

static void read_decision_cleanup(void * arg1, void * arg2){
	DecisionRead * ld = (DecisionRead * ) arg1;

	if (ld->n_events > 0){
		struct spdk_event * e = spdk_event_allocate(ld->target_core,read_decision_cleanup,ld,NULL);
		spdk_event_call(e);
	}
	else{
		delete ld;
	}
}

static void read_decision_completion(void *arg,const struct spdk_nvme_cpl *completion){
	DecisionReadOpt * ld_opt = (DecisionReadOpt *) arg;

	if (spdk_nvme_cpl_is_error(completion)) {
		fprintf(stderr, "I/O error status: %s read_decision_completion\n", spdk_nvme_cpl_get_status_string(&completion->status));
		fprintf(stderr, "Write I/O failed, aborting run\n");

		//SPDK_ENV::print_crtl_csts_status(ld_opt->disk_id);
		ld_opt->status = 2;
		exit(1);
	}
	DecisionRead * ld = ld_opt->dr;

	SPDK_ENV::SCHEDULE_EVENTS[ld_opt->target_core]--;

	if (ld->status){ //already ended the read from a majority of disks
		ld->n_events--;
		spdk_free(ld_opt->buffer);
		ld_opt->status = 1;
		return;
	}

	string db_serialized = bytes_to_string(ld_opt->buffer); //buffer to string
	DiskBlock db;
	db.deserialize(db_serialized); //inverse of serialize

	if (db.isValid()){
		ld->callback.set_value(db);
		ld->n_events--;
		spdk_free(ld_opt->buffer);
		ld_opt->status = 1;
		ld->status = 1;

		struct spdk_event * e = spdk_event_allocate(ld->target_core,read_decision_cleanup,ld,NULL);
		spdk_event_call(e);
	}
	else{
		read_decision_repeat(ld_opt);
	}
}

static void verify_event_decision(void * arg1, void * arg2){
	DecisionReadOpt * dO = (DecisionReadOpt *) arg1;

	map<string,unique_ptr<SPDK_ENV::NVME_NAMESPACE_MULTITHREAD>>::iterator it;
	it = SPDK_ENV::namespaces.find(dO->disk_id);

	if (!dO->status){
		map<uint32_t,struct spdk_nvme_qpair	*>::iterator it_qpair;
		it_qpair = it->second->qpairs.find(dO->target_core);

		spdk_nvme_qpair_process_completions(it_qpair->second, 1);

		struct spdk_event * e = spdk_event_allocate(dO->target_core,verify_event_decision,dO,NULL);
		spdk_event_call(e);
	}
	else{
		delete dO;
	}
}

static void read_decision_event(void * arg1,void * arg2){
	DecisionRead * ld = (DecisionRead *) arg1;

	map<string,unique_ptr<SPDK_ENV::NVME_NAMESPACE_MULTITHREAD>>::iterator it;
	map<uint32_t,struct spdk_nvme_qpair	*>::iterator it_qpair;

	int LBA_INDEX = SPDK_ENV::NUM_CONCENSOS_LANES * SPDK_ENV::NUM_PROCESSES + ld->target_slot;

	for(auto disk_id: SPDK_ENV::addresses){
		it = SPDK_ENV::namespaces.find(disk_id);

		if (it != SPDK_ENV::namespaces.end()){

			size_t BUFFER_SIZE = (it->second->info.lbaf + it->second->info.metadata_size);
			DecisionReadOpt * ld_opt = new DecisionReadOpt(disk_id,BUFFER_SIZE,ld->target_core,ld);

			ld_opt->buffer = (DiskPaxos::byte *) spdk_zmalloc(BUFFER_SIZE, 0x1000, NULL, SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);
			it_qpair = it->second->qpairs.find(ld_opt->target_core);

			if (ld_opt->buffer == NULL) //apagar depois
				fprintf(stderr, "NULL Buffer on read_decision_event\n");

			int rc = spdk_nvme_ns_cmd_read(it->second->ns, it_qpair->second , ld_opt->buffer,
								LBA_INDEX,
								1,
								read_decision_completion, ld_opt,0);

			if (rc != 0) {
				SPDK_ENV::error_on_cmd_submit(rc,"read_decision_event","read");
				exit(1);
			}

			SPDK_ENV::SCHEDULE_EVENTS[ld_opt->target_core]++;
			ld->n_events++;
			struct spdk_event * e = spdk_event_allocate(ld_opt->target_core,verify_event_decision,ld_opt,NULL);
			spdk_event_call(e);
		}
	}

}

std::future<DiskBlock> DiskPaxos::read_decision(int slot){
	uint32_t core = SPDK_ENV::allocate_replica_core();
	DecisionRead * dr = new DecisionRead(slot,core);

	dr->callback = promise<DiskBlock>();
	auto response = dr->callback.get_future();

	struct spdk_event * e = spdk_event_allocate(dr->target_core,read_decision_event,dr,NULL);
	spdk_event_call(e);

	return response;
}

std::future<DiskBlock> DiskPaxos::read_decision(int slot,uint32_t target_core){
	uint32_t core = target_core;
	DecisionRead * dr = new DecisionRead(slot,core);

	dr->callback = promise<DiskBlock>();
	auto response = dr->callback.get_future();

	struct spdk_event * e = spdk_event_allocate(dr->target_core,read_decision_event,dr,NULL);
	spdk_event_call(e);

	return response;
}

static void read_multiple_decision_cleanup(void * arg1, void * arg2){
	MultipleDecisionRead * ld = (MultipleDecisionRead * ) arg1;

	if (ld->n_events > 0){
		struct spdk_event * e = spdk_event_allocate(ld->target_core,read_multiple_decision_cleanup,ld,NULL);
		spdk_event_call(e);
	}
	else{
		delete ld;
	}
}

static void verify_event_multiple_decision(void * arg1, void * arg2){
	MultipleDecisionReadOpt * dO = (MultipleDecisionReadOpt *) arg1;

	map<string,unique_ptr<SPDK_ENV::NVME_NAMESPACE_MULTITHREAD>>::iterator it;
	it = SPDK_ENV::namespaces.find(dO->disk_id);

	if (!dO->status){
		map<uint32_t,struct spdk_nvme_qpair	*>::iterator it_qpair;
		it_qpair = it->second->qpairs.find(dO->target_core);

		spdk_nvme_qpair_process_completions(it_qpair->second, 1);

		struct spdk_event * e = spdk_event_allocate(dO->target_core,verify_event_multiple_decision,dO,NULL);
		spdk_event_call(e);
	}
	else{
		delete dO;
	}
}

static void read_multiple_decision_completion(void *arg,const struct spdk_nvme_cpl *completion){
	MultipleDecisionReadOpt * ld_opt = (MultipleDecisionReadOpt *) arg;

	if (spdk_nvme_cpl_is_error(completion)) {
		fprintf(stderr, "I/O error status: %s read_multiple_decision_completion\n", spdk_nvme_cpl_get_status_string(&completion->status));
		fprintf(stderr, "Read I/O failed, aborting run\n");

		//SPDK_ENV::print_crtl_csts_status(ld_opt->disk_id);
		ld_opt->status = 2;
		exit(1);
	}
	MultipleDecisionRead * ld = ld_opt->dr;
	SPDK_ENV::SCHEDULE_EVENTS[ld_opt->target_core]--;

	if (ld->status){ //already ended the read from a majority of disks
		ld->n_events--;
		spdk_free(ld_opt->buffer);
		ld_opt->status = 1;
		return;
	}

	map<int,DiskBlock>::iterator it;

	for(int i = 0; i < ld->amount; i++){
		it = ld->decisions.find(i + ld->starting_slot);
		if (it == ld->decisions.end()){
			string db_serialized = bytes_to_string(ld_opt->buffer + ld_opt->size_elem * i); //buffer to string
			DiskBlock db;
			db.deserialize(db_serialized); //inverse of serialize

			if (db.isValid()){
				ld->decisions.insert(pair<int,DiskBlock>(i + ld->starting_slot,db));
			}
		}
	}

	ld->disksSeen.insert(ld_opt->disk_id);

	if(ld->should_deliver()){
		ld->deliver();
	}

	ld->n_events--;
	spdk_free(ld_opt->buffer);
	ld_opt->status = 1;
}

static void read_multiple_decisions_event(void * arg1,void * arg2){
	MultipleDecisionRead * ld = (MultipleDecisionRead *) arg1;

	map<string,unique_ptr<SPDK_ENV::NVME_NAMESPACE_MULTITHREAD>>::iterator it;
	map<uint32_t,struct spdk_nvme_qpair	*>::iterator it_qpair;

	int LBA_INDEX = SPDK_ENV::NUM_CONCENSOS_LANES * SPDK_ENV::NUM_PROCESSES + ld->starting_slot;

	for(auto disk_id: SPDK_ENV::addresses){
		it = SPDK_ENV::namespaces.find(disk_id);

		if (it != SPDK_ENV::namespaces.end()){

			size_t BUFFER_SIZE = (it->second->info.lbaf + it->second->info.metadata_size);
			MultipleDecisionReadOpt * ld_opt = new MultipleDecisionReadOpt(disk_id,BUFFER_SIZE,ld->target_core,ld);

			ld_opt->buffer = (DiskPaxos::byte *) spdk_zmalloc(BUFFER_SIZE * ld->amount, 0x1000, NULL, SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);
			it_qpair = it->second->qpairs.find(ld_opt->target_core);

			if (ld_opt->buffer == NULL) //apagar depois
				fprintf(stderr, "NULL Buffer on read_multiple_decision_event\n");

			int rc = spdk_nvme_ns_cmd_read(it->second->ns, it_qpair->second , ld_opt->buffer,
								LBA_INDEX,
								ld->amount,
								read_multiple_decision_completion, ld_opt,0);

			if (rc != 0) {
				SPDK_ENV::error_on_cmd_submit(rc,"read_multiple_decision_event","read");
				exit(1);
			}

			SPDK_ENV::SCHEDULE_EVENTS[ld_opt->target_core]++;
			ld->n_events++;
			struct spdk_event * e = spdk_event_allocate(ld_opt->target_core,verify_event_multiple_decision,ld_opt,NULL);
			spdk_event_call(e);
		}
	}
}

std::future<std::unique_ptr<std::map<int,DiskBlock>>> DiskPaxos::read_multiple_decisions(int slot,int amount){
	uint32_t core = SPDK_ENV::allocate_replica_core();

	MultipleDecisionRead * m_dr = new MultipleDecisionRead(slot,amount,core);

	m_dr->callback = promise<unique_ptr< map<int,DiskBlock> >>();
	auto response = m_dr->callback.get_future();


	//spawn event to complete request
	struct spdk_event * e = spdk_event_allocate(m_dr->target_core,read_multiple_decisions_event,m_dr,NULL);
	spdk_event_call(e);

	return response;
}



namespace DiskPaxos {
	void launch_DiskPaxos(DiskPaxos * dp){
		dp->target_core = SPDK_ENV::allocate_leader_core();

		struct spdk_event * e = spdk_event_allocate(dp->target_core,spawn_disk_paxos,dp,NULL);
		spdk_event_call(e);
	}

	void launch_DiskPaxos(DiskPaxos * dp,uint32_t target_core){
		dp->target_core = target_core;

		struct spdk_event * e = spdk_event_allocate(dp->target_core,spawn_disk_paxos,dp,NULL);
		spdk_event_call(e);
	}
}
