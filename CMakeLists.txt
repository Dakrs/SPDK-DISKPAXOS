cmake_minimum_required(VERSION 3.10)

project(DiskPaxos)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} \
        -Wall \
        -Wextra \
        -Wno-unused-parameter \
        -Wno-missing-field-initializers \
        -Wmissing-declarations \
        -fno-strict-aliasing \
        -march=native \
        -Wformat \
        -Wformat-security \
        -D_GNU_SOURCE \
        -fPIC \
        -fstack-protector \
        -fno-common \
        -DNDEBUG \
        -O2 \
        -U_FORTIFY_SOURCE \
        -D_FORTIFY_SOURCE=2 \
        -pthread \
    ")

set(SPDK_DIR /home/gsd/spdk)
set(ENV{PKG_CONFIG_PATH} "${SPDK_DIR}/build/lib/pkgconfig")

message(STATUS "home: ${SPDK_DIR}")
message(STATUS "foo include dir: ${PKG_CONFIG_PATH}")

find_package(PkgConfig REQUIRED)
pkg_check_modules(SPDK_LIBS REQUIRED spdk_nvme spdk_vmd spdk_event)

include_directories (${SPDK_LIBS_INCLUDE_DIRS})
link_directories (${SPDK_LIBS_LIBRARY_DIRS})

add_executable(DiskPaxos
  src/main.cpp
  src/Disk/DiskBlock.cpp
  src/Command.cpp
  src/BankCommand.cpp
  src/Disk/DiskAccess.cpp
  src/Test/disk_isomorphic_test.cpp
  src/Disk/SPDK_ENV.cpp
  src/Processes/Leader.cpp
  src/Processes/Replica.cpp
  src/Disk/DiskPaxos.cpp
)

##### PROTOCOL BUFFERS ########
#find_package(Protobuf REQUIRED)
#target_link_libraries(DiskPaxos ${Protobuf_LIBRARIES})

add_definitions (${SPDK_LIBS_CFLAGS_OTHER})

target_include_directories (DiskPaxos PUBLIC include)
target_include_directories (DiskPaxos PUBLIC ../external)
target_include_directories (DiskPaxos PUBLIC ${SPDK_DIR}/include)

target_link_libraries (
  DiskPaxos
  -Wl,--whole-archive
  -Wl,--no-as-needed
  ${SPDK_LIBS_LIBRARIES}
  -Wl,--no-whole-archive
)

set(DPDK_LIBS
  ${SPDK_DIR}/dpdk/build/lib/librte_eal.a
  ${SPDK_DIR}/dpdk/build/lib/librte_mempool.a
  ${SPDK_DIR}/dpdk/build/lib/librte_ring.a
  ${SPDK_DIR}/dpdk/build/lib/librte_mbuf.a
  ${SPDK_DIR}/dpdk/build/lib/librte_bus_pci.a
  ${SPDK_DIR}/dpdk/build/lib/librte_pci.a
  ${SPDK_DIR}/dpdk/build/lib/librte_mempool_ring.a
  ${SPDK_DIR}/dpdk/build/lib/librte_power.a
  ${SPDK_DIR}/dpdk/build/lib/librte_ethdev.a
  ${SPDK_DIR}/dpdk/build/lib/librte_net.a
  ${SPDK_DIR}/dpdk/build/lib/librte_telemetry.a
  ${SPDK_DIR}/dpdk/build/lib/librte_kvargs.a
  ${SPDK_DIR}/dpdk/build/lib/librte_vhost.a
  ${SPDK_DIR}/dpdk/build/lib/librte_cryptodev.a
  ${SPDK_DIR}/dpdk/build/lib/librte_hash.a
  ${SPDK_DIR}/dpdk/build/lib/librte_rcu.a
)

link_directories (${SPDK_DIR}/dpdk/build/lib)

target_link_libraries (DiskPaxos
  -Wl,--no-whole-archive
  ${SPDK_DIR}/build/lib/libspdk_env_dpdk.a
  -Wl,--whole-archive
  ${DPDK_LIBS}
  -Wl,--no-whole-archive
)

## static system libs
target_link_libraries(DiskPaxos
  -L${SPDK_DIR}/isa-l/.libs
  -lisal
  -lrt
  -luuid
  -lcrypto
  -laio
  -lnuma
  -ldl
)


add_executable(Reset
  src/Scripts/reset.cpp
  src/Disk/DiskBlock.cpp
  src/Disk/DiskAccess.cpp
  src/Test/disk_isomorphic_test.cpp
)

target_include_directories (Reset PUBLIC include)
target_include_directories (Reset PUBLIC ../external)
target_include_directories (Reset PUBLIC ${SPDK_DIR}/include)

target_link_libraries (
  Reset
  -Wl,--whole-archive
  -Wl,--no-as-needed
  ${SPDK_LIBS_LIBRARIES}
  -Wl,--no-whole-archive
)

target_link_libraries (Reset
  -Wl,--no-whole-archive
  ${SPDK_DIR}/build/lib/libspdk_env_dpdk.a
  -Wl,--whole-archive
  ${DPDK_LIBS}
  -Wl,--no-whole-archive
)

target_link_libraries(Reset
  -L${SPDK_DIR}/isa-l/.libs
  -lisal
  -lrt
  -luuid
  -lcrypto
  -laio
  -lnuma
  -ldl
)

#[[
find_package(cppzmq)
target_link_libraries(DiskPaxos cppzmq)
add_definitions("-DZMQ_BUILD_DRAFT_API=1")

add_executable(ReplicaPaxos
  src/Replica.cpp
)

target_include_directories (ReplicaPaxos PUBLIC include)
target_link_libraries(ReplicaPaxos cppzmq)
target_link_libraries(ReplicaPaxos ${Protobuf_LIBRARIES})

add_executable(ServerPaxos
  src/Server.cpp
  src/message.pb.cc
)

target_include_directories (ServerPaxos PUBLIC include)
target_link_libraries(ServerPaxos cppzmq)
target_link_libraries(ServerPaxos ${Protobuf_LIBRARIES})

add_executable(ClientPaxos
  src/Client.cpp
  src/message.pb.cc
)

target_include_directories (ClientPaxos PUBLIC include)
target_link_libraries(ClientPaxos cppzmq)
target_link_libraries(ClientPaxos ${Protobuf_LIBRARIES})
]]#
