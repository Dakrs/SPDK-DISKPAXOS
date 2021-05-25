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
#include "Disk/SPDK_ENV.hpp"

using namespace std;

#define PROPOSAL_OFFSET 5000000

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

	LeaderRead(
		int slot,
		int n_slots,
		uint32_t target_core
	): starting_slot(slot), number_of_slots(n_slots), target_core(target_core){
		this->n_events = 0;
		this->status = 0;
	};
	~LeaderRead(){
		cout << "Cleaded" << endl;
	};
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

	if (size > 512){
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
	cout << "Starting consensus for slot: " << dp->slot << " on core: " << dp->target_core << endl;
	dp->startBallot();
}

void start_DiskPaxos(DiskPaxos::DiskPaxos * dp){
	dp->target_core = SPDK_ENV::allocate_leader_core();;

	struct spdk_event * e = spdk_event_allocate(dp->target_core,spawn_disk_paxos,dp,NULL);
	spdk_event_call(e);
}

void DiskPaxos::DiskPaxos::initPhase(){
	this->disksSeen.clear();
	this->blocksSeen.clear();

	DiskBlock * db = this->local_block->copy();
	this->blocksSeen.push_back(unique_ptr<DiskBlock>(db));
}

void DiskPaxos::DiskPaxos::startBallot(){
	cout << "Started a new Ballot" << endl;
	this->tick++;
	this->phase = 1;
	this->nextBallot += SPDK_ENV::NUM_PROCESSES;
	this->local_block->mbal = this->nextBallot;
	this->initPhase();
	this->ReadAndWrite();
}

void DiskPaxos::DiskPaxos::phase2(){
	cout << "Began phase " << this->phase << " N_E: " << this->n_events << endl;
	this->phase = 2;
	this->initPhase();
	this->ReadAndWrite();
}

void DiskPaxos::DiskPaxos::endPhase(){
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
			if (dp->disksSeen.size() > (SPDK_ENV::addresses.size() / 2) ){
				dp->endPhase();
			}
		}
	}

	spdk_free(dO->buffer);
	dO->status = 1;
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

	int rc = spdk_nvme_ns_cmd_read(it->second->ns, it_qpair->second , dO->buffer,
						LBA_INDEX, /* LBA start */
						SPDK_ENV::NUM_PROCESSES, /* number of LBAs */
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

void DiskPaxos::DiskPaxos::ReadAndWrite(){
	cout << "Started ReadAndWrite Phase: " << this->phase << endl;
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

			int rc = spdk_nvme_ns_cmd_write(it->second->ns, it_qpair->second , dO->buffer,
								LBA_INDEX, /* LBA start */
								1, /* number of LBAs */
								write_complete, dO, 0); //submit a write operation to NVME

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
	DiskPaxos::DiskPaxos * dp = (DiskPaxos::DiskPaxos *) arg1;

	if (dp->n_events > 0){
		//there are still events running
		//spawn event recurvely
		struct spdk_event * e = spdk_event_allocate(dp->target_core,cleanup,dp,NULL);
		spdk_event_call(e);
	}
	else{
		dp->finished = 1;
		cout << "Consensus finished" << endl;
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

void DiskPaxos::DiskPaxos::Abort(int mbal){
	int pid_responsible = mbal % SPDK_ENV::NUM_PROCESSES;
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

static void write_commit_complete(void *arg,const struct spdk_nvme_cpl *completion){
	DiskOperation * dO = (DiskOperation *) arg;

	if (spdk_nvme_cpl_is_error(completion)) {
		fprintf(stderr, "I/O error status: %s\n", spdk_nvme_cpl_get_status_string(&completion->status));
		fprintf(stderr, "Write I/O failed, aborting run\n");
		dO->status = 2;
		exit(1);
	}

	spdk_free(dO->buffer);
	cout << "Commit on: " << dO->disk_id << " started" << endl;
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
}

void DiskPaxos::DiskPaxos::Commit(){
	cout << "Consensus archived: " << this->local_block->input << endl;

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

			int rc = spdk_nvme_ns_cmd_write(it->second->ns, it_qpair->second , dO->buffer,
								LBA_INDEX, /* LBA start */
								1, /* number of LBAs */
								write_commit_complete, dO, 0); //submit a write operation to NVME

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

static void simple_write_complete(void *arg,const struct spdk_nvme_cpl *completion){
	DiskOperation * dO = (DiskOperation *) arg;

	if (spdk_nvme_cpl_is_error(completion)) {
		fprintf(stderr, "I/O error status: %s\n", spdk_nvme_cpl_get_status_string(&completion->status));
		fprintf(stderr, "Write I/O failed, aborting run\n");
		dO->status = 2;
		exit(1);
	}

	spdk_free(dO->buffer);
	dO->status = 1;
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

			int rc = spdk_nvme_ns_cmd_write(it->second->ns, it_qpair->second , dO->buffer,
								LBA_INDEX, /* LBA start */
								1, /* number of LBAs */
								simple_write_complete, dO, SPDK_NVME_IO_FLAGS_FORCE_UNIT_ACCESS); //submit a write operation to NVME

			if (rc != 0) {
					fprintf(stderr, "starting write I/O failed\n");
					exit(1);
			}

			struct spdk_event * e = spdk_event_allocate(dO->target_core,verify_event,dO,NULL);
			spdk_event_call(e);
		}
	}

	delete props;
}

void propose(int pid, int slot,string command){
	uint32_t core = SPDK_ENV::allocate_replica_core();
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

		spdk_nvme_qpair_process_completions(it_qpair->second, 0);

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

	if (spdk_nvme_cpl_is_error(completion)) {
		fprintf(stderr, "I/O error status: %s\n", spdk_nvme_cpl_get_status_string(&completion->status));
		fprintf(stderr, "Write I/O failed, aborting run\n");
		ld_opt->status = 2;
		exit(1);
	}

	LeaderRead * ld = ld_opt->ld;

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

	std::cout << "Read Proposal Completed" << '\n';
}

static void read_list_proposals(void * arg1,void * arg2){
	LeaderRead * ld = (LeaderRead *) arg1;

	map<string,unique_ptr<SPDK_ENV::NVME_NAMESPACE_MULTITHREAD>>::iterator it;
	map<uint32_t,struct spdk_nvme_qpair	*>::iterator it_qpair;

	int LBA_INDEX = SPDK_ENV::NUM_CONCENSOS_LANES * SPDK_ENV::NUM_PROCESSES + PROPOSAL_OFFSET + ld->starting_slot * SPDK_ENV::NUM_PROCESSES;

	for(auto disk_id: SPDK_ENV::addresses){
		it = SPDK_ENV::namespaces.find(disk_id);

		if (it != SPDK_ENV::namespaces.end()){

			size_t BUFFER_SIZE = (it->second->info.lbaf + it->second->info.metadata_size);
			LeaderReadOpt * ld_opt = new LeaderReadOpt(disk_id,BUFFER_SIZE,ld->target_core,ld);

			ld_opt->buffer = (DiskPaxos::byte *) spdk_zmalloc(BUFFER_SIZE * ld->number_of_slots * SPDK_ENV::NUM_PROCESSES, 0x1000, NULL, SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);
			it_qpair = it->second->qpairs.find(ld_opt->target_core);

			int rc = spdk_nvme_ns_cmd_read(it->second->ns, it_qpair->second , ld_opt->buffer,
								LBA_INDEX,
								SPDK_ENV::NUM_PROCESSES*ld->number_of_slots,
								leader_read_completion, ld_opt,0);

			if (rc != 0) {
					fprintf(stderr, "starting write I/O failed\n");
					exit(1);
			}

			ld->n_events++;
			struct spdk_event * e = spdk_event_allocate(ld_opt->target_core,verify_event_leader,ld_opt,NULL);
			spdk_event_call(e);
		}
	}

}

std::future<unique_ptr<map<int,DiskBlock>> > read_proposals(int k,int number_of_slots){
	uint32_t core = SPDK_ENV::allocate_leader_core();
	LeaderRead * ld = new LeaderRead(k,number_of_slots,core);

	ld->callback = promise<unique_ptr< map<int,DiskBlock> >>();
	auto response = ld->callback.get_future();

	struct spdk_event * e = spdk_event_allocate(ld->target_core,read_list_proposals,ld,NULL);
	spdk_event_call(e);

	return response;
}
