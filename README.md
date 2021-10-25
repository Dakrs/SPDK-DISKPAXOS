# A Disk Paxos Implementation using SPDK

Disk Paxos is a consensus protocol which relies on writes and reads in a shared network of disk. The repository contains an implementation using [Storage Performance Development Kit](https://spdk.io) (SPDK).

## Installation

```bash
sudo ./installDeps.sh -p INSTALL_PATH_TO_SPDK

cd INSTALL_PATH_TO_SPDK

sudo ./scripts/setup.sh

./build.sh -p INSTALL_PATH_TO_SPDK
```

## Usage

### Disk Setup

In each machine where a network disk should run, use the following command

```bash
cd src/Scripts/Remote

# Example
# sudo ./single_nvmf_tgt_setup.sh -p ~/spdk -d 0000:00:04:0 -n nqn.2016-06.io.spdk:cnode1 -i 35.225.240.68 -c 4420 -m 0x2
sudo ./single_nvmf_tgt_setup.sh -p SPDK_INSTALL_PATH -d PCI_ID -n NQN string -i IP_ADDRESS -c PORT -m CPU_MASK
```

Keep in mind that NQN Strings must be unique.

### Configure Benchmark

We will need two config files. The first one will have the details for the Replica process.

```json
{
    "name": "Name of the application",
    "qpair_queue_size": "DISK QUEUE SIZE",
    "qpair_queue_request": "N DISK QUEUE Requests",
    "proposal_interval": "Proposal Interval in microseconds",
    "read_amount_replica": "Number of slots each Replica reads in a request",
    "number_of_slots_to_read": "Number of Proposals each Leader reads each time",
    "strip_size": "Size of the stripe, 0 if not used"
}
```

Example:
```json
{
    "name": "diskpasso_test_script",
    "qpair_queue_size": "4096",
    "qpair_queue_request": "4096",
    "proposal_interval": "50",
    "read_amount_replica": "8",
    "number_of_slots_to_read": "4",
    "strip_size": "4"
}
```

After that we need a config file for the Benchmark.

```
{
  "devices": [  # List of each disk configure. Must have an entry for each disk configured with the same data
    {
      "diskid": "LOCAL PCI ID",
      "nqn": "NQN String",
      "ip": "IP",
      "port": "PORT",
      "cpumask": "CPU MASK",
      "name": "SSH CONNECTION STRING"
    }
  ],
  "processes": [ # List of each process to launch
    {
      "id": "ID must start at 0",
      "cpumask": "CPU MASK",
      "name": "SSH CONNECTION STRING",
      "config": "Name of the config file"
    }
  ],
  "trids": "trtype:TCP adrfam:IPv4 traddr:IP trsvcid:PORT subnqn:NQN STRING; ..."
  # Trids must contain that information for each device
}
```

Example for a network with 3 devices.

```json
{
  "devices": [
    {
      "diskid": "0000:00:04.0",
      "nqn": "nqn.2016-06.io.spdk:cnode1",
      "ip": "10.128.15.196",
      "port": "4420",
      "cpumask": "0x1",
      "name": "diogosobral98@instance-template-1.us-central1-a.diskpaxos"
    },
    {
      "diskid": "0000:00:04.0",
      "nqn": "nqn.2016-06.io.spdk:cnode2",
      "ip": "10.128.15.197",
      "port": "4420",
      "cpumask": "0x1",
      "name": "diogosobral98@instance-template-2.us-central1-a.diskpaxos"
    },
    {
      "diskid": "0000:00:04.0",
      "nqn": "nqn.2016-06.io.spdk:cnode3",
      "ip": "10.128.15.198",
      "port": "4420",
      "cpumask": "0x1",
      "name": "diogosobral98@instance-template-3.us-central1-a.diskpaxos"
    }
  ],
  "processes": [
    {
      "id": "0",
      "cpumask": "0x80",
      "name": "diogosobral98@instance-template-1.us-central1-a.diskpaxos",
      "config": "test_configuration.json"
    },
    {
      "id": "1",
      "cpumask": "0x80",
      "name": "diogosobral98@instance-template-2.us-central1-a.diskpaxos",
      "config": "test_configuration.json"
    },
    {
      "id": "2",
      "cpumask": "0x80",
      "name": "diogosobral98@instance-template-3.us-central1-a.diskpaxos",
      "config": "test_configuration.json"
    }
  ],
  "trids": "trtype:TCP adrfam:IPv4 traddr:10.128.15.196 trsvcid:4420 subnqn:nqn.2016-06.io.spdk:cnode1;trtype:TCP adrfam:IPv4 traddr:10.128.15.197 trsvcid:4420 subnqn:nqn.2016-06.io.spdk:cnode2;trtype:TCP adrfam:IPv4 traddr:10.128.15.198 trsvcid:4420 subnqn:nqn.2016-06.io.spdk:cnode3;"
}
```

### Launch Benchmark

```
./src/Scripts/Remote/benchmark.sh -p N_PROCESSES -n N_PROPOSALS -l N_LANES -m SSH_STRING_REPLICA_THAT_RESETS -c PATH_TO_BENCHMARK_FILE -t PATH_TO_PROCESS_CONFIGURATION
```

Example:
```
./src/Scripts/Remote/benchmark.sh
  -p 3
  -n 20000
  -l 16
  -c src/Scripts/Remote/disksconfig.json
  -m diogosobral98@instance-template-1.us-central1-a.diskpaxos
  -t src/Scripts/Remote/test_configuration.json
```

The results will be available in each machine at build/logs/*.log.
