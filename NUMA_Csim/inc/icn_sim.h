#ifndef ICN_SIM_H
#define ICN_SIM_H

#include <array>
#include <cmath>
#include <limits>
#include <vector>

#include "champsim_constants.h"
#include "delay_queue.hpp"
#include "memory_class.h"
#include "operable.h"
#include "util.h"

/*
 * Model link latency and bandwidth for socket to socekt interconnects
 */

/// this is in champsim_constants.h. Kept here for reference
// #define T_LOCAL 1
// #define T_1HOP  120 //50ns*2.4GHz, 50ns from 130ns - cache&memacc time(80)
// #define T_2HOP  672 //280ns*2.4GHz, 280ns from 360ns - cache&memacc time(80)
// #define T_CXL   288 //120ns*2.4GHz, 120ns from 200ns - cache&memacc time(80)

#define U64 uint64_t
#define DBGTRACK 0

#define MAX_LINK_ITER 12
#define MAX_LINK_COUNTER 2048 //giving large number for static array of pointers
#define LARGE_PKT_SIZE 64
#define SMALL_PKT_SIZE 8
#define REQ 0
#define RESP 1
#define STARVATION_CHECK_LIM 200 //arbitrarily set

#define BT_DELAY 60 //30ns delay for block transfer (directory and owner cache lookup)

//#define DELAY_Q_SIZE 128
#define DELAY_Q_SIZE 8192

class SS_LINK;

struct packet_route{
  PACKET packet;
  SS_LINK * links[MAX_LINK_ITER];
  bool qtype[MAX_LINK_ITER];
  bool isW;
  uint64_t num_links=1;
  uint64_t cur_link=0;
  uint64_t icn_entry_time=0;
  uint64_t address; //bring this out for eq_addr..
  uint64_t dest=0;

};
template <>
struct is_valid<packet_route> {
  bool operator()(const packet_route& test) { return test.address != 0; }
};

class SS_LINK {
  public:
    //uint64_t allaccs=0;
    uint64_t all_traffic_in_B=0;
    uint32_t allowed_accesses=100;
    uint32_t bw_stall = 0;
    uint32_t cur_accesses=0;
    uint32_t remaining_stall = 0;
    uint32_t starvation_check_reqQ = 0;
    uint32_t starvation_yields = 0;
    //uint32_t remaining_stall_RespQ = 0;
  #if DBGTRACK
    uint32_t rq_full_count=0;
    uint32_t wq_full_count=0;
    uint32_t sum_occupancy=0;
    uint32_t num_rq_enqs=0;
    uint32_t last_arrival=0;
    uint32_t sum_interval=0;
  #endif
    const uint32_t _latency;
    champsim::delay_queue<packet_route> reqQ{DELAY_Q_SIZE, _latency},
                                        respQ{DELAY_Q_SIZE, _latency};
    SS_LINK(unsigned latency): _latency(latency){};
};

class ICN_SIM : public champsim::operable, public MemoryRequestConsumer, public MemoryRequestProducer
{
public:
  const std::string NAME;
  uint64_t s_cycles=0;

  //stat
  uint64_t n_2hop = 0, n_1hop=0, n_local=0, n_CXO=0;
  // stats for socket 0 (i.e. champsim simulated cores)
  uint64_t S0_n_2hop = 0, S0_n_1hop=0, S0_n_local=0, S0_n_CXO=0;

  uint64_t n_block_transfers_fromhome=0, n_homeCXL_but_block_transfer=0;
  uint64_t n_S0_block_transfers_fromhome=0, n_S0_homeCXL_but_block_transfer=0;
  uint64_t n_block_transfers_case2=0, n_S0_block_transfers_case2=0;
  uint64_t n_block_transfers_icn=0, n_S0_block_transfers_icn=0;

  //// stats recorded at retiring ICN. ONLY FOR SOCKET 0
  uint64_t non_cxl_accs_ret=0, cxl_accs_ret=0;
  uint64_t non_cxl_bt_ret=0, cxl_bt_ret=0;
  uint64_t non_cxl_acc_latsum_ret=0, cxl_acc_latsum_ret=0;
  uint64_t non_cxl_bt_latsum_ret=0, cxl_bt_latsum_ret=0;

  uint64_t non_cxl_migration_ret=0, cxl_migration_ret=0;
  uint64_t non_cxl_migration_latsum_ret=0, cxl_migration_latsum_ret=0;

  // dbg stat - count num_links
  uint64_t non_cxl_accs_hopsum=0, cxl_accs_hopsum=0;
  uint64_t non_cxl_bt_hopsum=0, cxl_bt_hopsum=0;

  uint64_t sum_CXL_lat=0;
  uint64_t n_CXL_accs=0;
  // uint64_t sum_1hop_lat=0;
  // uint64_t n_1hop_accs=0;
  // uint64_t sum_remote_lat=0;
  // uint64_t n_remote_accs=0;

  //DBG
  uint64_t recvd_migration_lines=0;
  uint64_t merged_migration_lines=0;

  std::vector<std::vector<struct SS_LINK>> SS_LINKS;
  
  //std::vector<struct SS_LINK> CXL_LINKS;
  //std::vector<struct SS_LINK> UPI_TO_NUMA_LINKS;
  
  std::vector<struct SS_LINK> TO_CXL;
  std::vector<struct SS_LINK> FROM_CXL;
  std::vector<struct SS_LINK> TO_REMOTE;
  std::vector<struct SS_LINK> FROM_REMOTE;

  //struct SS_LINK * all_link_ptrs[MAX_LINK_COUNTER]={0};
  //uint64_t num_all_links=0;
  std::vector<struct SS_LINK *> all_link_ptrs={};

  champsim::delay_queue<packet_route> bt_delayQ{8192, BT_DELAY};

  int add_rq(PACKET* packet) override;
  int add_wq(PACKET* packet) override;
  int add_pq(PACKET* packet) override;
  void return_data(PACKET* packet) override;

  int link_enq(packet_route pr, SS_LINK * ss_l, bool isW, bool qtype);

  void operate() override;
  void operate_delay_qs();
  //void operate_channel();

  bool check_nextlink_cantake(packet_route& pr);
  void handle_resps_and_reqs();
  void handle_resps();
  void handle_reqs();
  void retire_bt_delay_q();

  uint32_t get_occupancy_icn(uint8_t queue_type, uint64_t address, PACKET hpkt);
  uint32_t get_occupancy(uint8_t queue_type, uint64_t address) override;
  uint32_t get_size_icn(uint8_t queue_type, uint64_t address, PACKET hpkt);
  uint32_t get_size(uint8_t queue_type, uint64_t address) override;
  uint32_t get_channel(uint64_t address); // must use same function as in memory controller side (Page allocation table lookup)
  packet_route generate_packet_route(PACKET packet, bool isW);
  uint32_t add_links_to_pr(packet_route &pr, uint32_t src, uint32_t dest, uint32_t & offset, bool qtype);
  bool can_take(uint64_t src, uint64_t dest, bool isW);
  int forward_packet_route(packet_route& pr, champsim::delay_queue<packet_route> & currQ);
  
  void PrintStats();
  void ResetStats();
  void print_reqQ(SS_LINK &sslink, uint64_t & inflight_accs, uint64_t iter, uint64_t iter2 );
  void print_respQ(SS_LINK &sslink, uint64_t & inflight_accs, uint64_t iter, uint64_t iter2 );
  void print_deadlock();
  
  U64 gethop(U64 a, U64 b){
    if(b==CXO){
      return 3;
    }
    if(a==CXO){ //idk if we'll have this case
      return 3;
    }
    if(a==b){
      return 0;
    }
    U64 agrp=(a>>2);
    U64 bgrp=(b>>2);
    if(agrp==bgrp){
      return 1;
    }
    return 2;
  }

  ICN_SIM(std::string name, double freq_scale, unsigned fill_level, MemoryRequestConsumer* ll)
          : champsim::operable(freq_scale), MemoryRequestConsumer(fill_level), 
            MemoryRequestProducer(ll), NAME(name)
  {
    // **** 1. Generate SS_LINKS (all socket to socket links, 1 hop or remote 2 hops) **** //
    for(unsigned i=0;i<N_SOCKETS;i++){
      std::vector<struct SS_LINK> tmp_links;
      for(unsigned j=0;j<N_SOCKETS;j++){
        U64 hops = gethop(i,j);
        U64 hoplat=1;
        //TODO set naccs according to available BW per link
        U64 naccs=1;
        U64 bw_stall=UPI_BW_INTERVAL;
        if(hops==0){
          hoplat=T_LOCAL;
          naccs=10000; //unlimited if local
          bw_stall=0;
        }
        if(hops==1){hoplat=T_1HOP;}
        if(hops==2){hoplat=T_2HOP;}
        if(hops==3){std::cout<<"ICN_SIM: at init, shouldn't get this"<<std::endl;}
        struct SS_LINK tmp_link(hoplat);
        tmp_link.allowed_accesses=naccs;
        tmp_link.bw_stall=bw_stall;
        tmp_links.push_back(tmp_link);
      }
      SS_LINKS.push_back(tmp_links);
    }

    // **** 2. Generate CXL_LINKS (to CXL and from CXL, w/ respect to each socket) **** //
    U64 cxl_naccs=8;//TODO update this accordingly
    for(unsigned i=0;i<N_SOCKETS;i++){
      struct SS_LINK tmp_link(T_CXL);
      tmp_link.allowed_accesses=cxl_naccs;
      tmp_link.bw_stall=CXL_BW_INTERVAL;
      TO_CXL.push_back(tmp_link);
    }
    for(unsigned i=0;i<N_SOCKETS;i++){
      struct SS_LINK tmp_link(T_CXL);
      tmp_link.allowed_accesses=cxl_naccs;
      tmp_link.bw_stall=CXL_BW_INTERVAL;
      FROM_CXL.push_back(tmp_link);
    }

    // **** 3. Generate UPI to/from remote links () **** //
    U64 upi_to_numa_link_nacc=1;
    for(unsigned i=0;i<N_SOCKETS;i++){
      struct SS_LINK tmp_link(T_1HOP);
      tmp_link.allowed_accesses=upi_to_numa_link_nacc;
      tmp_link.bw_stall=UPI_BW_INTERVAL;
      TO_REMOTE.push_back(tmp_link);
    }
    for(unsigned i=0;i<N_SOCKETS;i++){
      struct SS_LINK tmp_link(T_1HOP);
      tmp_link.allowed_accesses=upi_to_numa_link_nacc;
      tmp_link.bw_stall=UPI_BW_INTERVAL;
      FROM_REMOTE.push_back(tmp_link);
    }


    //populate pointers in all_link_ptrs
    for (auto& sslinkarr: SS_LINKS) {
      for(auto& sslink : sslinkarr){
        all_link_ptrs.push_back(&(sslink));
      }
    }
    for(auto& sslink : TO_CXL){
      all_link_ptrs.push_back(&(sslink));
    }
    for(auto& sslink : FROM_CXL){
      all_link_ptrs.push_back(&(sslink));
    }

    for(auto& sslink : TO_REMOTE){
      all_link_ptrs.push_back(&(sslink));
    }
    for(auto& sslink : FROM_REMOTE){
      all_link_ptrs.push_back(&(sslink));
    }

    std::cout << "Initialized Socket to Socket Interconnect Sim"<< std::endl;
  }
};

#endif
