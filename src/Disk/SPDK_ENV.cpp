#include "Disk/SPDK_ENV.hpp"

#include <atomic>
#include <thread>
#include <memory>
#include <iostream>
#include <algorithm>

namespace SPDK_ENV {

  int NUM_PROCESSES = 0;
  int NUM_CONCENSOS_LANES = 0;
  uint32_t NEXT_CORE = 0;
  uint32_t NEXT_CORE_REPLICA = 0;

  std::set<std::string> crtl_addresses;
  std::set<std::string> addresses; //set a string identifying every disk
  std::vector<std::unique_ptr<NVME_CONTROLER_V2>> controllers; //vector with all controller structures allocated
  std::map<std::string,std::unique_ptr<NVME_NAMESPACE_MULTITHREAD>> namespaces; // map of disk id to working namespace

  std::atomic<bool> ready(false); //flag to make sure spdk libray has started
  std::thread internal_spdk_event_launcher; //internal thread to coordinate spdk.

  static bool probe_cb(void *cb_ctx,const struct spdk_nvme_transport_id *trid,struct spdk_nvme_ctrlr_opts *opts){
    /**
    std::string addr(trid->traddr);
    const bool is_in = crtl_addresses.find(addr) != crtl_addresses.end();

    if (!is_in){
      crtl_addresses.insert(addr);
      std::cout << "Device on addr: " << addr << std::endl;
      return true;
    }*/
    return true;
  }
  static int register_ns(struct spdk_nvme_ctrlr *ctrlr,struct spdk_nvme_ns *ns,const struct spdk_nvme_transport_id *trid,int nsid){
    if (!spdk_nvme_ns_is_active(ns)) {
  		return -1;
  	}

    NVME_NAMESPACE_MULTITHREAD * my_ns = new NVME_NAMESPACE_MULTITHREAD(ctrlr,ns);

  	uint32_t n_cores = spdk_env_get_core_count();
  	uint32_t k = spdk_env_get_first_core();
  	for (uint32_t i = 0; i < n_cores; i++) {
  		my_ns->qpairs.insert(std::pair<uint32_t,struct spdk_nvme_qpair	*>(k,spdk_nvme_ctrlr_alloc_io_qpair(ctrlr, NULL, 0)));
  		k = spdk_env_get_next_core(k);
  	}

    std::string addr;

    if (strlen(trid->subnqn) == 0){
      std::string addr_aux(trid->traddr);
      addr = addr_aux + "-NS:" + std::to_string(nsid);
    }
    else{
      addr = std::string(trid->subnqn);
    }

    addresses.insert(addr);
    namespaces.insert(std::pair<std::string,std::unique_ptr<NVME_NAMESPACE_MULTITHREAD>>(addr,std::unique_ptr<NVME_NAMESPACE_MULTITHREAD>(my_ns)));

    printf("  Namespace ID: %d size: %juGB %lu %s\n", spdk_nvme_ns_get_id(ns),
           spdk_nvme_ns_get_size(ns) / 1000000000, spdk_nvme_ns_get_num_sectors(ns),addr.c_str());

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
  		register_ns(ctrlr, ns, trid,nsid);
      //if (!res)
        //break; // só quero o primeiro namespace
  	}

    controllers.push_back(std::unique_ptr<NVME_CONTROLER_V2>(current_ctrlr));
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

  static bool probe_nvmf(std::vector<std::string> * trids){
    int successfull_hooks = 0;
    struct spdk_nvme_transport_id connect_id;

    for(auto s_trids : (*trids)){
      connect_id = {};
      const char * trid = s_trids.c_str();
      int trid_flag = spdk_nvme_transport_id_parse(&connect_id,trid);
  		if (trid_flag != 0){
  			fprintf(stderr, "spdk_nvme_transport_id_parse() failed\n");
        continue;
  		}

      std::cout << "trid: " << trid << '\n';

      int rc = spdk_nvme_probe(&connect_id, NULL, probe_cb, attach_cb, NULL);

      if (rc != 0) {
    		fprintf(stderr, "spdk_nvme_probe() failed\n");
    	}
      else{
        successfull_hooks++;
      }
    }

    return successfull_hooks > 0 && trids->size() > 0;
  }

  static void app_init(void * arg){
    int rc;
    if (arg == NULL){
      std::cout << "Connecting to local device" << std::endl;
      rc = spdk_nvme_probe(NULL, NULL, probe_cb, attach_cb, NULL);

      if (rc != 0) {
        fprintf(stderr, "spdk_nvme_probe() failed\n");
        spdk_app_stop(0);
      }
    }
    else{
      std::vector<std::string> * trids = (std::vector<std::string> *) arg;
      bool status = probe_nvmf(trids);

      if (!status) {
        fprintf(stderr, "spdk_nvme_probe() failed\n");
        spdk_app_stop(0);
      }
    }

  	NEXT_CORE = spdk_env_get_first_core();
  	NEXT_CORE_REPLICA = spdk_env_get_first_core();

  	ready = true;

  	std::cout << "Succeded: CPU_CORES -> " << spdk_env_get_core_count() << std::endl;
  }

  static void run_spdk_event_framework(const char * CPU_MASK){
    struct spdk_app_opts app_opts = {};
    spdk_app_opts_init(&app_opts, sizeof(app_opts));
  	app_opts.name = "event_test";
  	app_opts.reactor_mask = CPU_MASK;

    int rc = spdk_app_start(&app_opts, app_init, NULL);

  	if (rc){
  		std::cout << "Error might have occured" << std::endl;
  	}

  	cleanup();
  	//spdk_vmd_fini();

  	spdk_app_fini();

    ready = false;
  }


  static void run_spdk_event_framework_nvmf(const char * CPU_MASK, std::vector<std::string> * trids){
    struct spdk_app_opts app_opts = {};
    spdk_app_opts_init(&app_opts, sizeof(app_opts));
    app_opts.name = "event_test";
    app_opts.reactor_mask = CPU_MASK;

    int rc = spdk_app_start(&app_opts, app_init, trids);

    if (rc){
      std::cout << "Error might have occured" << std::endl;
    }

    cleanup();
    //spdk_vmd_fini();

    spdk_app_fini();
  }

  int spdk_start(int n_p,int n_k,const char * CPU_MASK,std::vector<std::string>& trids){
    NUM_PROCESSES = n_p;
  	NUM_CONCENSOS_LANES = n_k;

    std::vector<std::string> * v_aux = new std::vector<std::string> ();
    for(auto trid : trids)
      v_aux->push_back(trid);

    internal_spdk_event_launcher = std::thread(run_spdk_event_framework_nvmf,CPU_MASK, v_aux);

    while(!ready);

    return 0;
  }

  int spdk_start(int n_p,int n_k,const char * CPU_MASK) {

  	NUM_PROCESSES = n_p;
  	NUM_CONCENSOS_LANES = n_k;

  	internal_spdk_event_launcher = std::thread(run_spdk_event_framework,CPU_MASK);

  	while(!ready);

    return 0;
  }

  void spdk_end() {
    std::cout << "Shutting down SPDK Framework" << std::endl;
  	spdk_app_stop(0);
  	internal_spdk_event_launcher.join();
  }

  uint32_t allocate_leader_core(){
    uint32_t core = NEXT_CORE;
    NEXT_CORE = spdk_env_get_next_core(NEXT_CORE);

    if (NEXT_CORE == UINT32_MAX){
      NEXT_CORE = spdk_env_get_first_core();
    }
    return core;
  }

  uint32_t allocate_replica_core(){
    uint32_t core = NEXT_CORE_REPLICA;
    NEXT_CORE_REPLICA = spdk_env_get_next_core(NEXT_CORE_REPLICA);

    if (NEXT_CORE_REPLICA == UINT32_MAX){
      NEXT_CORE_REPLICA = spdk_env_get_first_core();
    }
    return core;
  }

  void print_addresses(){
    std::cout << "Addresses: " << std::endl;
    for(auto str: addresses){
      std::cout << str << ' ';
    }
    std::cout << std::endl;
  }

  void error_on_cmd_submit(int code, std::string func, std::string type){
    switch (code) {
      case -EINVAL:
        fprintf(stderr, "starting %s I/O failed on %s because The request is malformed\n",type.c_str(),func.c_str());
        break;
      case -ENOMEM:
        fprintf(stderr, "starting %s I/O failed on %s because The request cannot be allocated\n",type.c_str(),func.c_str());
        break;
      case -ENXIO:
        fprintf(stderr, "starting %s I/O failed on %s because The qpair is failed at the transport level\n",type.c_str(),func.c_str());
        break;
      case -EFAULT:
        fprintf(stderr, "starting %s I/O failed on %s because Invalid address was specified as part of payload\n",type.c_str(),func.c_str());
        break;
      default:
        break;
    }
  }

  void print_crtl_csts_status(std::string diskid){
    std::map<std::string,std::unique_ptr<NVME_NAMESPACE_MULTITHREAD>>::iterator it;
    it = namespaces.find(diskid);

    struct spdk_nvme_ctrlr	* ctrl = it->second->ctrlr;

    union spdk_nvme_csts_register reg = spdk_nvme_ctrlr_get_regs_csts(ctrl);

    std::cout << "Disk: " << diskid << " rdy: " << reg.bits.rdy << " cfs: " << reg.bits.cfs << '\n';
  }

  void print_qpair_failure_reason(std::string diskid,uint32_t core){
    std::map<std::string,std::unique_ptr<NVME_NAMESPACE_MULTITHREAD>>::iterator it;
    it = namespaces.find(diskid);

    std::map<uint32_t,struct spdk_nvme_qpair	*>::iterator it_qpair;
    it_qpair = it->second->qpairs.find(core);

    spdk_nvme_qp_failure_reason reason = spdk_nvme_qpair_get_failure_reason(it_qpair->second);

    switch (reason) {
      case SPDK_NVME_QPAIR_FAILURE_NONE:
        std::cout << "Error SPDK_NVME_QPAIR_FAILURE_NONE" << std::endl;
        break;
      case SPDK_NVME_QPAIR_FAILURE_LOCAL:
        std::cout << "Error SPDK_NVME_QPAIR_FAILURE_LOCAL" << std::endl;
        break;
      case SPDK_NVME_QPAIR_FAILURE_REMOTE:
        std::cout << "Error SPDK_NVME_QPAIR_FAILURE_REMOTE" << std::endl;
        break;
      case SPDK_NVME_QPAIR_FAILURE_UNKNOWN:
        std::cout << "Error SPDK_NVME_QPAIR_FAILURE_UNKNOWN" << std::endl;
        break;
      default:
        std::cout << "Error not found" << std::endl;
    }
  }
}
