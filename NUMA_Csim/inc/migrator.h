/*
 * Operable to drive migration at the start of the simulated phase
 */

#ifndef MIGRATOR_H
#define MIGRATOR_H

#include <array>
#include <functional>
#include <queue>
#include <cstring>
#include <sstream>
#include <map>

#include "block.h"
#include "champsim.h"
#include "delay_queue.hpp"
#include "instruction.h"
#include "memory_class.h"
#include "operable.h"
//#include "champsim_constants.h"

#define MODEL_MIGRATION_OVERHEAD 1
#define MODEL_TLB_SD_OVERHEAD 1
#define BLOCKS_PER_PAGE 64

#define MIGRATION_INSID 0xc0ffee

using namespace std;

class MIGRATOR;

class MIGRATORBus : public MemoryRequestProducer
{
public:
  champsim::circular_buffer<PACKET> PROCESSED;
  MIGRATORBus(std::size_t q_size, MemoryRequestConsumer* ll, MIGRATOR * mparent) : MemoryRequestProducer(ll), PROCESSED(q_size), parent(mparent) {}
  uint64_t lat_hist[100]={0};
  void return_data(PACKET* packet);
  void print_lat_hist();
  uint64_t cur_cycle=0;
  uint64_t total_lat=0;
  uint64_t allaccs=0;
  MIGRATOR * parent;
};

// migrator - can be considered type of dummy core..
class MIGRATOR : public champsim::operable
{
public:
  uint32_t cpu = 200; //gave a random value

  bool migration_called=false;

  // instruction

  PACKET curpacket;

  //uint64_t INSTRUCTIONS_PER_PHASE=1000000000;
  //uint64_t cur_migration_phase=0;
  //uint64_t next_migration_i_count=INSTRUCTIONS_PER_PHASE;


  MIGRATORBus L1D_bus;
  std::map<uint64_t, std::pair<uint64_t, uint64_t>> pages_to_migrate;
  //std::vector<uint64_t> migration_lines;
  std::array<std::vector<uint64_t>, N_SOCKETS+1> migration_lines;
  //in flight - 0 if nothing in flight. Else, holds the page number of in-flight page
  std::array<uint64_t, N_SOCKETS+1> mig_in_flight={};
  std::array<uint64_t, N_SOCKETS+1> in_flights_start_time={};
  std::array<uint64_t, N_SOCKETS+1> in_flight_remaining_pages={};
  bool TLB_shotdown[NUM_CPUS]={};

  uint64_t n_migrated_pages=0; 
  uint64_t migration_time_sum=0; 
  //the first migration average includes those for CXL
  uint64_t n_migrated_pages_CXL=0;
  uint64_t migration_time_sum_CXL=0;
  //bool migration_all_done=false;
  uint64_t n_denied_access_to_mirating_page=0; // could have multiple blocks for same access

  void operate();

  int check_and_action();
  int start_migration();
  int populate_migration_lines();
  int send_migration_lines();
  int shootdown_TLB(uint64_t vaddr, uint64_t old_paddr);  
  int probe_TLBs(uint64_t vaddr);
 
  void handle_memory_return();

  //void print_deadlock() override;

  //int pack_curpacket(uint64_t buf_val);
  void print_lat_hist();

  int forward_to_roi(uint64_t target_i_count);
  uint64_t get_remaining_lines();

  MIGRATOR(double freq_scale, MemoryRequestConsumer* l1d)
      : champsim::operable(freq_scale), L1D_bus(2048, l1d, this)
  {
    //std::string mtrace_path="./";
    //std::ostringstream tfname;
		//tfname << mtrace_path << "/memtrace_t" << (cpu) << ".out";
		//std::cout<<tfname.str()<<std::endl;

    std::cout<<"MIGRATOR initilaized"<<std::endl;
    std::cout<<"MODEL_MIGRATION_OVERHEAD set to "<<MODEL_MIGRATION_OVERHEAD<<std::endl;
  }
};

#endif
