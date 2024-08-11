/*
 * Dummy TestBench CPU
 *  should read memory trace and drive them 
 *  at approriate timing
 */

#ifndef TB_CPU_H
#define TB_CPU_H

#include <array>
#include <functional>
#include <queue>
#include <cstring>
#include <sstream>

#include "block.h"
#include "champsim.h"
#include "delay_queue.hpp"
#include "instruction.h"
#include "memory_class.h"
#include "operable.h"
#include "coh_directory.h"

using namespace std;

#define LIM_OUTSTANDING_READS 128 //match ooo_core LQ size
#define THR_OFFSET_VAL 0

#define TB_LLC_SETS 8192
#define TB_LLC_WAYS 16

//discrepancy between champsim trace and memory trace for TB_CPUs
//#define FORWARD_INSTS 20000000000

//class CACHE;

typedef struct tb_cache_entry {
    uint64_t tag;   //addr
    bool valid=0;                 // valid bit
    bool dirty=0;                 // dirty bit
    coh_state_t cstate;      // coherence state
    uint64_t ts; //timestamp
} tb_cache_entry_t;

// typedef struct cache_set {
//     cache_entry_t entries[NUM_WAYS];
//     std::list<uint8_t> lru_list;
// } cache_set_t;

typedef struct tb_cache {
    tb_cache_entry_t entries[TB_LLC_SETS][TB_LLC_WAYS];
    //std::list<uint32_t> lru_list[NUM_SETS];
    //uint32_t socket_id;
    uint64_t hits=0, misses=0, evicts=0;
} tb_cache_t;
uint64_t access_tb_cache(tb_cache_t& cach, uint64_t lineaddr, bool isW, uint64_t ts, uint32_t socketid);
uint64_t inval_tb_cache(tb_cache_t& cach, uint64_t lineaddr, uint32_t socketid);

class TBCacheBus : public MemoryRequestProducer
{
public:
  champsim::circular_buffer<PACKET> PROCESSED;
  TBCacheBus(std::size_t q_size, MemoryRequestConsumer* ll, uint64_t pcpu) : MemoryRequestProducer(ll), PROCESSED(q_size), parent_cpu(pcpu) {}
  uint64_t lat_hist[100]={0};
  void return_data(PACKET* packet);
  void print_lat_hist();
  uint64_t cur_cycle=0;
  uint64_t total_lat=0;
  uint64_t allaccs=0;
  uint64_t outstanding_reads=0;
  uint64_t parent_cpu;
};

// cpu
class TB_CPU : public champsim::operable
{
public:
  uint32_t cpu = 0;

  FILE * trace;
  std::ostringstream tfname;
  
  uint64_t FORWARD_INSTS=0;

  // instruction
  uint64_t begin_sim_cycle = 0, last_sim_cycle = 0, 
           finish_sim_cycle = 0,
           num_reqs = 0;
  uint32_t inflight_mem_executions = 0;

  PACKET curpacket;
  uint64_t next_issue_i_count=0;
  uint64_t consecutive_rob_full_count=0;

  uint64_t load_count=0, store_count=0;
  uint64_t cxl_access=0, noncxl_access=0;

  TBCacheBus L1D_bus;

  void operate();

  int check_and_action();
 
  void handle_memory_return();
  void print_deadlock();
  //void print_deadlock() override;

  int read_8B_line(uint64_t * buf_val, char* buffer, FILE* fptr){
    size_t readsize = std::fread(buffer, sizeof(char), sizeof(buffer), fptr);
    if(readsize!=8) return -1;
    //dunno why but this step needed for things t work
    std::memcpy(buf_val, buffer, sizeof(uint64_t)); 
    return readsize;
  }
  int pack_curpacket(uint64_t buf_val);
  int send_curpacket();
  uint64_t dummy_memreq_gen();
  void print_lat_hist();

  int forward_to_roi(uint64_t tb_cpu_forawrd, uint64_t warmup_instructions);

  int open_trace(std::string mtrace_path, uint64_t n_phase){
    //std::ostringstream tfname;
		//tfname << mtrace_path << "/memtrace_t" << (cpu) << ".out";
    tfname << mtrace_path << "/mtrace_t" << (cpu)<<"_"<<n_phase << ".out";
		if(cpu==(NUM_CPUS+NUM_TBCPU-1)){
    std::cout<<tfname.str()<<std::endl;
    }
    trace = fopen(tfname.str().c_str(), "rb");
    if(trace==NULL){
      cout<<"Failed to open file "<<tfname.str()<<", abort"<<std::endl;
      exit(-1);
    }

    return 0;
  }

  TB_CPU(uint32_t in_cpu, double freq_scale, MemoryRequestConsumer* l1d)
      : champsim::operable(freq_scale), cpu(in_cpu+NUM_CPUS), L1D_bus(256, l1d, (in_cpu+NUM_CPUS))
  {
    if(in_cpu==(NUM_TBCPU-1)){
      std::cout<<"TB_CPU "<<in_cpu << " initilaized"<<std::endl;
    }
    uint64_t buf_val = rand();
    pack_curpacket(buf_val);

  }
};

#endif
