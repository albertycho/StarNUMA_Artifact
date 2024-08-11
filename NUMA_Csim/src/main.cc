/*
 *    Copyright 2023 The ChampSim Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <algorithm>
#include <array>
#include <fstream>
#include <functional>
#include <getopt.h>
#include <iomanip>
#include <signal.h>
#include <string.h>
#include <vector>

#include "cache.h"
#include "champsim.h"
#include "champsim_constants.h"
// #include "dram_controller.h"
#include "icn_sim.h"
#include "migrator.h"
#include "dramsim3_wrapper.hpp"
#include "hybrid_mem_wrapper.hpp"
#include "ooo_cpu.h"
#include "tb_cpu.h"
#include "operable.h"
#include "tracereader.h"
#include "vmem.h"
#include "coh_directory.h"
#include "ptw.h"

uint8_t warmup_complete[NUM_CPUS] = {}, simulation_complete[NUM_CPUS] = {}, all_warmup_complete = 0, all_simulation_complete = 0,
        MAX_INSTR_DESTINATIONS = NUM_INSTR_DESTINATIONS, knob_cloudsuite = 0, knob_low_bandwidth = 0;

uint64_t warmup_instructions = 1000000, simulation_instructions = 10000000;

// not modeling core stall from TLB shootdown anymore, but keeping datastructure for pacing migration
uint64_t TLB_shootdown_stall[NUM_CPUS]={};
//uint64_t wait_till_next_TLB_shootdown[NUM_CPUS]={};
// using this to pace migration. rate limits migration more if any access gets blocked
uint64_t wait_till_next_TLB_shootdown=0;
bool migration_all_done=false;

coh_directory coh_dir;
tb_cache_t tb_llcs[N_SOCKETS]={};

auto start_time = time(NULL);

// For backwards compatibility with older module source.
champsim::deprecated_clock_cycle current_core_cycle;

// extern MEMORY_CONTROLLER DRAM;
//extern DRAMSim3_DRAM DRAM;
extern HMEM DRAM;
extern ICN_SIM ICN;
extern VirtualMemory vmem;
extern std::array<O3_CPU*, NUM_CPUS> ooo_cpu;
extern std::array<CACHE*, NUM_CACHES> caches;
extern std::array<champsim::operable*, NUM_OPERABLES> operables;
extern std::array<TB_CPU*, NUM_TBCPU> tbcpus;
//extern TB_CPU tbcpu0;
//extern TB_CPU tbcpu1;
extern MIGRATOR mig0;

uint64_t n_completed_translation[NUM_CPUS];


std::vector<tracereader*> traces;

uint64_t champsim::deprecated_clock_cycle::operator[](std::size_t cpu_idx)
{
  static bool deprecate_printed = false;
  if (!deprecate_printed) {
    std::cout << "WARNING: The use of 'current_core_cycle[cpu]' is deprecated." << std::endl;
    std::cout << "WARNING: Use 'this->current_cycle' instead." << std::endl;
    deprecate_printed = true;
  }
  return ooo_cpu[cpu_idx]->current_cycle;
}

void record_roi_stats(uint32_t cpu, CACHE* cache)
{
  for (uint32_t i = 0; i < NUM_TYPES; i++) {
    cache->roi_access[cpu][i] = cache->sim_access[cpu][i];
    cache->roi_hit[cpu][i] = cache->sim_hit[cpu][i];
    cache->roi_miss[cpu][i] = cache->sim_miss[cpu][i];
  }
}

void print_roi_stats(uint32_t cpu, CACHE* cache)
{
  uint64_t TOTAL_ACCESS = 0, TOTAL_HIT = 0, TOTAL_MISS = 0;

  for (uint32_t i = 0; i < NUM_TYPES; i++) {
    TOTAL_ACCESS += cache->roi_access[cpu][i];
    TOTAL_HIT += cache->roi_hit[cpu][i];
    TOTAL_MISS += cache->roi_miss[cpu][i];
  }

  if (TOTAL_ACCESS > 0) {
    cout << cache->NAME;
    cout << " TOTAL     ACCESS: " << setw(10) << TOTAL_ACCESS << "  HIT: " << setw(10) << TOTAL_HIT << "  MISS: " << setw(10) << TOTAL_MISS << endl;

    cout << cache->NAME;
    cout << " LOAD      ACCESS: " << setw(10) << cache->roi_access[cpu][0] << "  HIT: " << setw(10) << cache->roi_hit[cpu][0] << "  MISS: " << setw(10)
         << cache->roi_miss[cpu][0] << endl;

    cout << cache->NAME;
    cout << " RFO       ACCESS: " << setw(10) << cache->roi_access[cpu][1] << "  HIT: " << setw(10) << cache->roi_hit[cpu][1] << "  MISS: " << setw(10)
         << cache->roi_miss[cpu][1] << endl;

    cout << cache->NAME;
    cout << " PREFETCH  ACCESS: " << setw(10) << cache->roi_access[cpu][2] << "  HIT: " << setw(10) << cache->roi_hit[cpu][2] << "  MISS: " << setw(10)
         << cache->roi_miss[cpu][2] << endl;

    cout << cache->NAME;
    cout << " WRITEBACK ACCESS: " << setw(10) << cache->roi_access[cpu][3] << "  HIT: " << setw(10) << cache->roi_hit[cpu][3] << "  MISS: " << setw(10)
         << cache->roi_miss[cpu][3] << endl;

    cout << cache->NAME;
    cout << " TRANSLATION ACCESS: " << setw(10) << cache->roi_access[cpu][4] << "  HIT: " << setw(10) << cache->roi_hit[cpu][4] << "  MISS: " << setw(10)
         << cache->roi_miss[cpu][4] << endl;

    cout << cache->NAME;
    cout << " PREFETCH  REQUESTED: " << setw(10) << cache->pf_requested << "  ISSUED: " << setw(10) << cache->pf_issued;
    cout << "  USEFUL: " << setw(10) << cache->pf_useful << "  USELESS: " << setw(10) << cache->pf_useless << endl;

    cout << cache->NAME;
    cout << " AVERAGE MISS LATENCY: " << (1.0 * (cache->total_miss_latency[cpu])) / TOTAL_MISS << " cycles" << endl;

    //LLC miss lat HISTOGRAM
	  if (cache->NAME == "LLC") {
		cout << "LLC_MISS_LAT_HIST (in ns):"<< endl;
		for(uint32_t i =0;i<100;i++){
        	cout<<i*10<<" : "<<cache->lat_hist[cpu][i]<<endl;
    	}

	}
    // cout << " AVERAGE MISS LATENCY: " <<
    // (cache->total_miss_latency)/TOTAL_MISS << " cycles " <<
    // cache->total_miss_latency << "/" << TOTAL_MISS<< endl;
  }
}

void print_sim_stats(uint32_t cpu, CACHE* cache)
{
  uint64_t TOTAL_ACCESS = 0, TOTAL_HIT = 0, TOTAL_MISS = 0;

  for (uint32_t i = 0; i < NUM_TYPES; i++) {
    TOTAL_ACCESS += cache->sim_access[cpu][i];
    TOTAL_HIT += cache->sim_hit[cpu][i];
    TOTAL_MISS += cache->sim_miss[cpu][i];
  }

  if (TOTAL_ACCESS > 0) {
    cout << cache->NAME;
    cout << " TOTAL     ACCESS: " << setw(10) << TOTAL_ACCESS << "  HIT: " << setw(10) << TOTAL_HIT << "  MISS: " << setw(10) << TOTAL_MISS << endl;

    cout << cache->NAME;
    cout << " LOAD      ACCESS: " << setw(10) << cache->sim_access[cpu][0] << "  HIT: " << setw(10) << cache->sim_hit[cpu][0] << "  MISS: " << setw(10)
         << cache->sim_miss[cpu][0] << endl;

    cout << cache->NAME;
    cout << " RFO       ACCESS: " << setw(10) << cache->sim_access[cpu][1] << "  HIT: " << setw(10) << cache->sim_hit[cpu][1] << "  MISS: " << setw(10)
         << cache->sim_miss[cpu][1] << endl;

    cout << cache->NAME;
    cout << " PREFETCH  ACCESS: " << setw(10) << cache->sim_access[cpu][2] << "  HIT: " << setw(10) << cache->sim_hit[cpu][2] << "  MISS: " << setw(10)
         << cache->sim_miss[cpu][2] << endl;

    cout << cache->NAME;
    cout << " WRITEBACK ACCESS: " << setw(10) << cache->sim_access[cpu][3] << "  HIT: " << setw(10) << cache->sim_hit[cpu][3] << "  MISS: " << setw(10)
         << cache->sim_miss[cpu][3] << endl;
  }
}

void print_branch_stats()
{
  for (uint32_t i = 0; i < NUM_CPUS; i++) {
    cout << endl << "CPU " << i << " Branch Prediction Accuracy: ";
    cout << (100.0 * (ooo_cpu[i]->num_branch - ooo_cpu[i]->branch_mispredictions)) / ooo_cpu[i]->num_branch;
    cout << "% MPKI: " << (1000.0 * ooo_cpu[i]->branch_mispredictions) / (ooo_cpu[i]->num_retired - warmup_instructions);
    cout << " Average ROB Occupancy at Mispredict: " << (1.0 * ooo_cpu[i]->total_rob_occupancy_at_branch_mispredict) / ooo_cpu[i]->branch_mispredictions
         << endl;

    /*
    cout << "Branch types" << endl;
    cout << "NOT_BRANCH: " << ooo_cpu[i]->total_branch_types[0] << " " <<
    (100.0*ooo_cpu[i]->total_branch_types[0])/(ooo_cpu[i]->num_retired -
    ooo_cpu[i]->begin_sim_instr) << "%" << endl; cout << "BRANCH_DIRECT_JUMP: "
    << ooo_cpu[i]->total_branch_types[1] << " " <<
    (100.0*ooo_cpu[i]->total_branch_types[1])/(ooo_cpu[i]->num_retired -
    ooo_cpu[i]->begin_sim_instr) << "%" << endl; cout << "BRANCH_INDIRECT: " <<
    ooo_cpu[i]->total_branch_types[2] << " " <<
    (100.0*ooo_cpu[i]->total_branch_types[2])/(ooo_cpu[i]->num_retired -
    ooo_cpu[i]->begin_sim_instr) << "%" << endl; cout << "BRANCH_CONDITIONAL: "
    << ooo_cpu[i]->total_branch_types[3] << " " <<
    (100.0*ooo_cpu[i]->total_branch_types[3])/(ooo_cpu[i]->num_retired -
    ooo_cpu[i]->begin_sim_instr) << "%" << endl; cout << "BRANCH_DIRECT_CALL: "
    << ooo_cpu[i]->total_branch_types[4] << " " <<
    (100.0*ooo_cpu[i]->total_branch_types[4])/(ooo_cpu[i]->num_retired -
    ooo_cpu[i]->begin_sim_instr) << "%" << endl; cout << "BRANCH_INDIRECT_CALL:
    " << ooo_cpu[i]->total_branch_types[5] << " " <<
    (100.0*ooo_cpu[i]->total_branch_types[5])/(ooo_cpu[i]->num_retired -
    ooo_cpu[i]->begin_sim_instr) << "%" << endl; cout << "BRANCH_RETURN: " <<
    ooo_cpu[i]->total_branch_types[6] << " " <<
    (100.0*ooo_cpu[i]->total_branch_types[6])/(ooo_cpu[i]->num_retired -
    ooo_cpu[i]->begin_sim_instr) << "%" << endl; cout << "BRANCH_OTHER: " <<
    ooo_cpu[i]->total_branch_types[7] << " " <<
    (100.0*ooo_cpu[i]->total_branch_types[7])/(ooo_cpu[i]->num_retired -
    ooo_cpu[i]->begin_sim_instr) << "%" << endl << endl;
    */

    cout << "Branch type MPKI" << endl;
    cout << "BRANCH_DIRECT_JUMP: " << (1000.0 * ooo_cpu[i]->branch_type_misses[1] / (ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr)) << endl;
    cout << "BRANCH_INDIRECT: " << (1000.0 * ooo_cpu[i]->branch_type_misses[2] / (ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr)) << endl;
    cout << "BRANCH_CONDITIONAL: " << (1000.0 * ooo_cpu[i]->branch_type_misses[3] / (ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr)) << endl;
    cout << "BRANCH_DIRECT_CALL: " << (1000.0 * ooo_cpu[i]->branch_type_misses[4] / (ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr)) << endl;
    cout << "BRANCH_INDIRECT_CALL: " << (1000.0 * ooo_cpu[i]->branch_type_misses[5] / (ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr)) << endl;
    cout << "BRANCH_RETURN: " << (1000.0 * ooo_cpu[i]->branch_type_misses[6] / (ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr)) << endl << endl;
  }
}

// void print_dram_stats()
// {
//   uint64_t total_congested_cycle = 0;
//   uint64_t total_congested_count = 0;

//   std::cout << std::endl;
//   std::cout << "DRAM Statistics" << std::endl;
//   for (uint32_t i = 0; i < DRAM_CHANNELS; i++) {
//     std::cout << " CHANNEL " << i << std::endl;

//     auto& channel = DRAM.channels[i];
//     std::cout << " RQ ROW_BUFFER_HIT: " << std::setw(10) << channel.RQ_ROW_BUFFER_HIT << " ";
//     std::cout << " ROW_BUFFER_MISS: " << std::setw(10) << channel.RQ_ROW_BUFFER_MISS;
//     std::cout << std::endl;

//     std::cout << " DBUS AVG_CONGESTED_CYCLE: ";
//     if (channel.dbus_count_congested)
//       std::cout << std::setw(10) << ((double)channel.dbus_cycle_congested / channel.dbus_count_congested);
//     else
//       std::cout << "-";
//     std::cout << std::endl;

//     std::cout << " WQ ROW_BUFFER_HIT: " << std::setw(10) << channel.WQ_ROW_BUFFER_HIT << " ";
//     std::cout << " ROW_BUFFER_MISS: " << std::setw(10) << channel.WQ_ROW_BUFFER_MISS << " ";
//     std::cout << " FULL: " << std::setw(10) << channel.WQ_FULL;
//     std::cout << std::endl;

//     std::cout << std::endl;

//     total_congested_cycle += channel.dbus_cycle_congested;
//     total_congested_count += channel.dbus_count_congested;
//   }

//   if (DRAM_CHANNELS > 1) {
//     std::cout << " DBUS AVG_CONGESTED_CYCLE: ";
//     if (total_congested_count)
//       std::cout << std::setw(10) << ((double)total_congested_cycle / total_congested_count);
//     else
//       std::cout << "-";

//     std::cout << std::endl;
//   }
// }

void reset_cache_stats(uint32_t cpu, CACHE* cache)
{
  for (uint32_t i = 0; i < NUM_TYPES; i++) {
    cache->sim_access[cpu][i] = 0;
    cache->sim_hit[cpu][i] = 0;
    cache->sim_miss[cpu][i] = 0;
  }

  cache->pf_requested = 0;
  cache->pf_issued = 0;
  cache->pf_useful = 0;
  cache->pf_useless = 0;
  cache->pf_fill = 0;

  //cache->total_miss_latency = 0;
  for(uint32_t i=0; i< NUM_CPUS;i++){
  	cache->total_miss_latency[i] = 0;
  }


  cache->RQ_ACCESS = 0;
  cache->RQ_MERGED = 0;
  cache->RQ_TO_CACHE = 0;

  cache->WQ_ACCESS = 0;
  cache->WQ_MERGED = 0;
  cache->WQ_TO_CACHE = 0;
  cache->WQ_FORWARD = 0;
  cache->WQ_FULL = 0;
}

void finish_warmup()
{
  uint64_t elapsed_second = (uint64_t)(time(NULL) - start_time), elapsed_minute = elapsed_second / 60, elapsed_hour = elapsed_minute / 60;
  elapsed_minute -= elapsed_hour * 60;
  elapsed_second -= (elapsed_hour * 3600 + elapsed_minute * 60);

  // reset core latency
  // note: since re-ordering he function calls in the main simulation loop, it's
  // no longer necessary to add
  //       extra latency for scheduling and execution, unless you want these
  //       steps to take longer than 1 cycle.
  // PAGE_TABLE_LATENCY = 100;
  // SWAP_LATENCY = 100000;

  cout << endl;
  for (uint32_t i = 0; i < NUM_CPUS; i++) {
    cout << "Warmup complete CPU " << i << " instructions: " << ooo_cpu[i]->num_retired << " cycles: " << ooo_cpu[i]->current_cycle;
    cout << " (Simulation time: " << elapsed_hour << " hr " << elapsed_minute << " min " << elapsed_second << " sec) " << endl;

    ooo_cpu[i]->begin_sim_cycle = ooo_cpu[i]->current_cycle;
    ooo_cpu[i]->begin_sim_instr = ooo_cpu[i]->num_retired;

    // reset branch stats
    ooo_cpu[i]->num_branch = 0;
    ooo_cpu[i]->branch_mispredictions = 0;
    ooo_cpu[i]->total_rob_occupancy_at_branch_mispredict = 0;

    for (uint32_t j = 0; j < 8; j++) {
      ooo_cpu[i]->total_branch_types[j] = 0;
      ooo_cpu[i]->branch_type_misses[j] = 0;
    }

    for (auto it = caches.rbegin(); it != caches.rend(); ++it)
      reset_cache_stats(i, *it);
  }
  cout << endl;
  //check LLC warm status:
  cout<<caches[0]->NAME<<endl;
  caches[0]->get_warm_status();
  for(int i=1;i<N_SOCKETS;i++){
    uint64_t all_blocks=TB_LLC_SETS*TB_LLC_WAYS;
    uint64_t valid_blocks=0;
    for(int j=0; j<TB_LLC_SETS;j++){
      for(int k=0; k<TB_LLC_WAYS;k++){
        if(tb_llcs[i].entries[j][k].valid){
          valid_blocks++;
        }
      }
    }
    uint64_t warm_rate = valid_blocks * 100 / all_blocks;
    cout<<"Socket"<<i<<" LLC is "<<warm_rate<<"% warm ("<<valid_blocks<<"/"<<all_blocks<<")"<<endl;
  }

  // reset DRAM stats
  // for (uint32_t i = 0; i < DRAM_CHANNELS; i++) {
  //   DRAM.channels[i].WQ_ROW_BUFFER_HIT = 0;
  //   DRAM.channels[i].WQ_ROW_BUFFER_MISS = 0;
  //   DRAM.channels[i].RQ_ROW_BUFFER_HIT = 0;
  //   DRAM.channels[i].RQ_ROW_BUFFER_MISS = 0;
  // }
}

void signal_handler(int signal)
{
  cout << "Caught signal: " << signal << endl;
  exit(1);
}

int main(int argc, char** argv)
{
  // interrupt signal hanlder
  struct sigaction sigIntHandler;
  sigIntHandler.sa_handler = signal_handler;
  sigemptyset(&sigIntHandler.sa_mask);
  sigIntHandler.sa_flags = 0;
  sigaction(SIGINT, &sigIntHandler, NULL);

  cout << endl << "*** ChampSim Multicore Out-of-Order Simulator ***" << endl << endl;

  // initialize knobs
uint8_t show_heartbeat = 1;
uint64_t tb_cpu_forward = 0;  // default value
std::string tb_cpu_trace_path = ".";  // default value

// check to see if knobs changed using getopt_long()
int traces_encountered = 0;
static struct option long_options[] = {
    {"warmup_instructions", required_argument, 0, 'w'},
    {"simulation_instructions", required_argument, 0, 'i'},
    {"hide_heartbeat", no_argument, 0, 'h'},
    {"cloudsuite", no_argument, 0, 'c'},
    {"tb_cpu_forward", required_argument, 0, 'f'},  // new option
    {"tb_cpu_trace_path", required_argument, 0, 'p'},  // new option
    {"traces", no_argument, &traces_encountered, 1},
    {0, 0, 0, 0}
};

int c;
while ((c = getopt_long_only(argc, argv, "w:i:hcf:p:", long_options, NULL)) != -1 && !traces_encountered) {
    switch (c) {
    case 'w':
        warmup_instructions = atol(optarg);
        //dbg
        std::cout << "Warmup_instructions registered, " << warmup_instructions << std::endl;
        break;
    case 'i':
        simulation_instructions = atol(optarg);
        break;
    case 'h':
        show_heartbeat = 0;
        break;
    case 'c':
        knob_cloudsuite = 1;
        MAX_INSTR_DESTINATIONS = NUM_INSTR_DESTINATIONS_SPARC;
        break;
    case 'f':  // new option
        tb_cpu_forward = atoll(optarg);
        break;
    case 'p':  // new option
        tb_cpu_trace_path = optarg;
        break;
    case 0:
        break;
    default:
        abort();
    }
}


  cout << "Warmup Instructions: " << warmup_instructions << endl;
  cout << "Simulation Instructions: " << simulation_instructions << endl;
  cout << "Number of CPUs: " << NUM_CPUS << endl;
  cout <<"TB_CPU forward: "<< tb_cpu_forward<<endl;
  cout <<"TB_CPU path: "<< tb_cpu_trace_path<<endl;

  for(auto tmp_tbcpu : tbcpus){
    //dbg
    cout<<"TBCPU's cpu field: "<<tmp_tbcpu->cpu<<endl;
    uint64_t n_phase = tb_cpu_forward/1000000000;
    tmp_tbcpu->open_trace(tb_cpu_trace_path, n_phase);
    tmp_tbcpu->forward_to_roi(tb_cpu_forward,warmup_instructions);
  }

  // long long int dram_size = DRAM_CHANNELS * DRAM_RANKS * DRAM_BANKS * DRAM_ROWS * DRAM_COLUMNS * BLOCK_SIZE / 1024 / 1024; // in MiB
  // std::cout << "Off-chip DRAM Size: ";
  // if (dram_size > 1024)
  //   std::cout << dram_size / 1024 << " GiB";
  // else
  //   std::cout << dram_size << " MiB";
  // std::cout << " Channels: " << DRAM_CHANNELS << " Width: " << 8 * DRAM_CHANNEL_WIDTH << "-bit Data Rate: " << DRAM_IO_FREQ << " MT/s" << std::endl;
  //std::cout << " CXL_MODE: " << CXL_MODE << std::endl;
  std::cout << std::endl;
  std::cout << "VirtualMemory physical capacity per socket: " << std::size(vmem.ppage_free_lists[0]) * vmem.page_size * CH_PER_SOCKET;
  std::cout << " num_ppages per socket: " << std::size(vmem.ppage_free_lists[0]) * CH_PER_SOCKET << std::endl;
  std::cout << "VirtualMemory page size: " << PAGE_SIZE << " log2_page_size: " << LOG2_PAGE_SIZE << std::endl;

  std::cout << std::endl;
  for (int i = optind; i < argc; i++) {
    std::cout << "CPU " << traces.size() << " runs " << argv[i] << std::endl;

    traces.push_back(get_tracereader(argv[i], traces.size(), knob_cloudsuite));

    if (traces.size() > NUM_CPUS) {
      printf("\n*** Too many traces for the configured number of cores ***\n\n");
      assert(0);
    }
  }

  if (traces.size() != NUM_CPUS) {
    printf("\n*** Not enough traces for the configured number of cores ***\n\n");
    assert(0);
  }
  // end trace file setup

  // SHARED CACHE
  for (O3_CPU* cpu : ooo_cpu) {
    cpu->initialize_core();
  }

  for (auto it = caches.rbegin(); it != caches.rend(); ++it) {
    (*it)->impl_prefetcher_initialize();
    (*it)->impl_replacement_initialize();
  }

  // simulation entry point
  while (std::any_of(std::begin(simulation_complete), std::end(simulation_complete), std::logical_not<uint8_t>())) {

    uint64_t elapsed_second = (uint64_t)(time(NULL) - start_time), elapsed_minute = elapsed_second / 60, elapsed_hour = elapsed_minute / 60;
    elapsed_minute -= elapsed_hour * 60;
    elapsed_second -= (elapsed_hour * 3600 + elapsed_minute * 60);

    for (auto op : operables) {
      try {
        op->_operate();
      } catch (champsim::deadlock& dl) {
        // ooo_cpu[dl.which]->print_deadlock();
        // std::cout << std::endl;
        // for (auto c : caches)
        for (auto c : operables) {
          c->print_deadlock();
          std::cout << std::endl;
        }

        abort();
      }
    }
    std::sort(std::begin(operables), std::end(operables), champsim::by_next_operate());

    for (std::size_t i = 0; i < ooo_cpu.size(); ++i) {
      // read from trace
      while (ooo_cpu[i]->fetch_stall == 0 && ooo_cpu[i]->instrs_to_read_this_cycle > 0) {
        ooo_cpu[i]->init_instruction(traces[i]->get());
      }

      // heartbeat information
      if (show_heartbeat && (ooo_cpu[i]->num_retired >= ooo_cpu[i]->next_print_instruction)) {
        float cumulative_ipc;
        if (warmup_complete[i])
          cumulative_ipc = (1.0 * (ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr)) / (ooo_cpu[i]->current_cycle - ooo_cpu[i]->begin_sim_cycle);
        else
          cumulative_ipc = (1.0 * ooo_cpu[i]->num_retired) / ooo_cpu[i]->current_cycle;
        float heartbeat_ipc = (1.0 * ooo_cpu[i]->num_retired - ooo_cpu[i]->last_sim_instr) / (ooo_cpu[i]->current_cycle - ooo_cpu[i]->last_sim_cycle);

        cout << "Heartbeat CPU " << i << " instructions: " << ooo_cpu[i]->num_retired << " cycles: " << ooo_cpu[i]->current_cycle;
        cout << " heartbeat IPC: " << heartbeat_ipc << " cumulative IPC: " << cumulative_ipc;
        cout << " (Simulation time: " << elapsed_hour << " hr " << elapsed_minute << " min " << elapsed_second << " sec) " << endl;
        //DBG
        if(warmup_complete[i]){
        cout <<"TLB shootdown stalls: "<<ooo_cpu[i]->TLB_sd_stalls<<endl;
        if(i==0){
          ICN.PrintStats();
          cout<<"MIGRATOR DEBUG"<<endl;
          cout<<"completed migration lines       : "<<mig0.L1D_bus.allaccs<<endl;
          cout<<"remaining lines                 : "<<mig0.get_remaining_lines()<<std::endl;
          cout<<"migration lines recvd by icn_sim: "<<ICN.recvd_migration_lines<<endl;
		  cout<<"Request stalls from migration in progress: "<< mig0.n_denied_access_to_mirating_page<<endl;
          for(uint64_t iii=0; iii<NUM_CPUS;iii++){
            cout<<"completed_translations from cpu"<<iii<<": "<<n_completed_translation[iii]<<endl;
          }
          //following segment gets broken, printing garbage into output file
          // uint64_t ptw_index_start = NUM_CPUS*3+1; //(cpus,l1d,l2c)*NUM_CPUS, + 1 for LLC
          // for(uint64_t iii=0; iii<NUM_CPUS;iii++){
          //   auto operable = operables[ptw_index_start-1+iii];
          //   PageTableWalker* recast_operable = (PageTableWalker*)operable;
          //   uint64_t n_comp_translations = recast_operable->n_completed_translation;
          //   cout<<"completed_translations from "<<recast_operable->NAME<<" "<<n_comp_translations<<endl;
          // }
  

          mig0.print_lat_hist();
          coh_dir.debug_print();
#if (NUM_TBCPU>0)
          tbcpus[0]->print_lat_hist();
#endif
        }
        }
        //cout<<"migration merged by icn_sim     : "<<ICN.merged_migration_lines<<endl;
        cout<<endl;
        // if(caches[0]->NAME =="LLC"){
        //   cout<<"MLP HIST"<<std::endl;
        //   for(int iii=0;iii<300;iii++){
        //     if(caches[0]->mlp_hist[i][iii]!=0){
        //       cout<<iii<<" : "<<caches[0]->mlp_hist[i][iii]<<std::endl;
        //     }
        //   }
        // }
        // else{cout<<"THIS IS NOT LLC"<<std::endl;}
        ooo_cpu[i]->next_print_instruction += STAT_PRINTING_PERIOD;

        ooo_cpu[i]->last_sim_instr = ooo_cpu[i]->num_retired;
        ooo_cpu[i]->last_sim_cycle = ooo_cpu[i]->current_cycle;
      }

      // check for warmup
      // warmup complete
      if ((warmup_complete[i] == 0) && (ooo_cpu[i]->num_retired > warmup_instructions)) {
        warmup_complete[i] = 1;
        all_warmup_complete++;
      }
      if (all_warmup_complete == NUM_CPUS) { // this part is called only once
                                             // when all cores are warmed up
        all_warmup_complete++;
        finish_warmup();
      }

      // simulation complete
      if ((all_warmup_complete > NUM_CPUS) && (simulation_complete[i] == 0)
          && (ooo_cpu[i]->num_retired >= (ooo_cpu[i]->begin_sim_instr + simulation_instructions))) {
        simulation_complete[i] = 1;
        ooo_cpu[i]->finish_sim_instr = ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr;
        ooo_cpu[i]->finish_sim_cycle = ooo_cpu[i]->current_cycle - ooo_cpu[i]->begin_sim_cycle;

        cout << "Finished CPU " << i << " instructions: " << ooo_cpu[i]->finish_sim_instr << " cycles: " << ooo_cpu[i]->finish_sim_cycle;
        cout << " cumulative IPC: " << ((float)ooo_cpu[i]->finish_sim_instr / ooo_cpu[i]->finish_sim_cycle);
        cout << " (Simulation time: " << elapsed_hour << " hr " << elapsed_minute << " min " << elapsed_second << " sec) " << endl;

        for (auto it = caches.rbegin(); it != caches.rend(); ++it)
          record_roi_stats(i, *it);
      }
    }
  }

  uint64_t elapsed_second = (uint64_t)(time(NULL) - start_time), elapsed_minute = elapsed_second / 60, elapsed_hour = elapsed_minute / 60;
  elapsed_minute -= elapsed_hour * 60;
  elapsed_second -= (elapsed_hour * 3600 + elapsed_minute * 60);

  cout << endl << "ChampSim completed all CPUs" << endl;
  if (NUM_CPUS > 1) {
    cout << endl << "Total Simulation Statistics (not including warmup)" << endl;
    for (uint32_t i = 0; i < NUM_CPUS; i++) {
      cout << endl
           << "CPU " << i
           << " cumulative IPC: " << (float)(ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr) / (ooo_cpu[i]->current_cycle - ooo_cpu[i]->begin_sim_cycle);
      cout << " instructions: " << ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr
           << " cycles: " << ooo_cpu[i]->current_cycle - ooo_cpu[i]->begin_sim_cycle << endl;
      for (auto it = caches.rbegin(); it != caches.rend(); ++it)
        print_sim_stats(i, *it);
    }
  }

  cout << endl << "Region of Interest Statistics" << endl;
  for (uint32_t i = 0; i < NUM_CPUS; i++) {
    cout << endl << "CPU " << i << " cumulative IPC: " << ((float)ooo_cpu[i]->finish_sim_instr / ooo_cpu[i]->finish_sim_cycle);
    cout << " instructions: " << ooo_cpu[i]->finish_sim_instr << " cycles: " << ooo_cpu[i]->finish_sim_cycle << endl;
    for (auto it = caches.rbegin(); it != caches.rend(); ++it)
      print_roi_stats(i, *it);
  }

  for (auto it = caches.rbegin(); it != caches.rend(); ++it)
    (*it)->impl_prefetcher_final_stats();

  for (auto it = caches.rbegin(); it != caches.rend(); ++it)
    (*it)->impl_replacement_final_stats();

#ifndef CRC2_COMPILE
  // print_dram_stats();
  DRAM.PrintStats();
  //print_branch_stats();
  ICN.PrintStats();
  //tbcpu0.print_lat_hist();
  //tbcpu1.print_lat_hist();
  mig0.print_lat_hist();
  cout<<"vp to socket found vs not found"<<vmem.vp_to_socket_found<<", "<<vmem.vp_to_socket_not_found<<std::endl;
  //print MLPhist
  if(caches[0]->NAME =="LLC"){
    cout<<"Core0 MLP HIST"<<std::endl;
    for(int iii=0;iii<300;iii++){
      if(caches[0]->mlp_hist[0][iii]!=0){
        cout<<iii<<" : "<<caches[0]->mlp_hist[0][iii]<<std::endl;
      }
    }
  }
  else{cout<<"THIS IS NOT LLC"<<std::endl;}
#endif

  return 0;
}
