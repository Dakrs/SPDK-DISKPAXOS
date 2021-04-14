
########### SPDK #################
SPDK_DIR=~/spdk
PKG_CONFIG_PATH=$(SPDK_DIR)/build/lib/pkgconfig
SYS_LIB := $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs --static spdk_syslibs)
DPDK_LIB_MAN = -Wl,--no-whole-archive $(SPDK_DIR)/build/lib/libspdk_env_dpdk.a -Wl,-rpath-link $(SPDK_DIR)/dpdk/build/lib -Wl,--whole-archive $(SPDK_DIR)/dpdk/build/lib/librte_eal.a $(SPDK_DIR)/dpdk/build/lib/librte_mempool.a $(SPDK_DIR)/dpdk/build/lib/librte_ring.a $(SPDK_DIR)/dpdk/build/lib/librte_mbuf.a $(SPDK_DIR)/dpdk/build/lib/librte_bus_pci.a $(SPDK_DIR)/dpdk/build/lib/librte_pci.a $(SPDK_DIR)/dpdk/build/lib/librte_mempool_ring.a $(SPDK_DIR)/dpdk/build/lib/librte_power.a $(SPDK_DIR)/dpdk/build/lib/librte_ethdev.a $(SPDK_DIR)/dpdk/build/lib/librte_net.a $(SPDK_DIR)/dpdk/build/lib/librte_telemetry.a $(SPDK_DIR)/dpdk/build/lib/librte_kvargs.a $(SPDK_DIR)/dpdk/build/lib/librte_vhost.a $(SPDK_DIR)/dpdk/build/lib/librte_cryptodev.a $(SPDK_DIR)/dpdk/build/lib/librte_hash.a $(SPDK_DIR)/dpdk/build/lib/librte_rcu.a -Wl,--no-whole-archive
SPDK_LIBS := $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs spdk_nvme spdk_vmd)
SPDK_LIBS_FLAGS := -L$(SPDK_DIR)/build/lib -Wl,--whole-archive -Wl,--no-as-needed $(SPDK_LIBS) -Wl,--no-whole-archive



CFLAGS = -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Wmissing-declarations -fno-strict-aliasing -march=native -Wformat -Wformat-security -D_GNU_SOURCE -fPIC -fstack-protector -fno-common -DNDEBUG -O2 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2 -pthread -L$(SPDK_DIR)/isa-l/.libs -std=c++11
CXX = g++

INCLUDES = -I include -I $(SPDK_DIR)/include -I ../external

SRC := src
OBJ := obj

SOURCES := $(wildcard $(SRC)/*.cpp)
OBJECTS := $(patsubst $(SRC)/%.cpp, $(OBJ)/%.o, $(SOURCES))

NAME = main

all: checkdirs BUILD

BUILD: $(OBJECTS)
	$(CXX) $(CFLAGS) -o $(NAME) $(OBJECTS) -pthread $(SPDK_LIBS_FLAGS)  $(DPDK_LIB_MAN) $(SYS_LIB)
	@echo Compiled without errors

$(OBJ)/%.o: $(SRC)/%.cpp
	$(CXX) $(CFLAGS) $(INCLUDES) -c $< -o $@

checkdirs:
	@mkdir -p $(OBJ)

clean:
	$(RM) -r $(OBJ)
	$(RM) $(NAME)
