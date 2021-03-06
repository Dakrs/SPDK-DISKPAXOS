cmake_minimum_required(VERSION 3.10)

project(DiskPaxos)

message(STATUS "SPDK DIR:${SPDK_P}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} \
        -Wall \
        -Wextra \
        -Wno-unused-parameter \
        -Wunused-function \
        -Wno-missing-field-initializers \
        -Wno-pointer-arith \
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
        -g \
        -fno-omit-frame-pointer \
        -U_FORTIFY_SOURCE \
        -D_FORTIFY_SOURCE=2 \
        -pthread \
        -fpermissive \
    ")

set(SPDK_DIR "${SPDK_P}")
set(ENV{PKG_CONFIG_PATH} "${SPDK_DIR}/build/lib/pkgconfig")

find_package(PkgConfig REQUIRED)
pkg_check_modules(SPDK_LIBS REQUIRED spdk_nvme spdk_vmd spdk_event)

include_directories (${SPDK_LIBS_INCLUDE_DIRS})
link_directories (${SPDK_LIBS_LIBRARY_DIRS})

add_definitions (${SPDK_LIBS_CFLAGS_OTHER})

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

add_executable(DiskPaxos_LocalThread
  src/Executables/local_thread_test.cpp
  src/Disk/DiskBlock.cpp
  src/Disk/DiskAccess.cpp
  src/Test/disk_isomorphic_test.cpp
  src/Disk/SPDK_ENV.cpp
  src/Processes/Leader.cpp
  src/Processes/Replica.cpp
  src/Disk/DiskPaxos.cpp
)

add_executable(Reset
  src/Executables/reset.cpp
  src/Disk/DiskBlock.cpp
  src/Disk/DiskAccess.cpp
  src/Test/disk_isomorphic_test.cpp
)

function(include_spdk TARGET_NAME)
  target_include_directories (${TARGET_NAME} PUBLIC include)
  target_include_directories (${TARGET_NAME} PUBLIC ${SPDK_DIR}/include)

  target_link_libraries (
    ${TARGET_NAME}
    -Wl,--whole-archive
    -Wl,--no-as-needed
    ${SPDK_LIBS_LIBRARIES}
    -Wl,--no-whole-archive
  )

  target_link_libraries (${TARGET_NAME}
    -Wl,--no-whole-archive
    ${SPDK_DIR}/build/lib/libspdk_env_dpdk.a
    -Wl,--whole-archive
    ${DPDK_LIBS}
    -Wl,--no-whole-archive
  )

  target_link_libraries(${TARGET_NAME}
    -L${SPDK_DIR}/isa-l/.libs
    -lisal
    -lrt
    -luuid
    -lcrypto
    -laio
    -lnuma
    -ldl
  )
endfunction()

add_executable(DiskPaxos_SimpleProcess
  src/Executables/single_process.cpp
  src/Disk/DiskBlock.cpp
  src/Disk/DiskPaxos.cpp
  src/Disk/SglOpts.cpp
  src/Disk/SPDK_ENV.cpp
  src/Processes/Leader.cpp
  src/Processes/Replica.cpp
  src/Processes/MultiReplica.cpp
)

add_executable(DiskPaxos_Bench
  src/Executables/benchleader.cpp
  src/Disk/DiskBlock.cpp
  src/Disk/DiskPaxos.cpp
  src/Disk/SglOpts.cpp
  src/Disk/SPDK_ENV.cpp
  src/Processes/Leader.cpp
  src/Processes/Replica.cpp
  src/Processes/MultiReplica.cpp
)

include_spdk(DiskPaxos_SimpleProcess)
include_spdk(DiskPaxos_Bench)
include_spdk(DiskPaxos_LocalThread)
include_spdk(Reset)
