/*
 * Dummy TestBench CPU
 *  should read memory trace and drive them 
 *  at approriate timing
 */

#ifndef COH_DIR_H
#define COH_DIR_H

#include <array>
#include <functional>
#include <cstring>
#include <sstream>
#include "champsim_constants.h"

#define ENABLE_COHERENCE 1

using namespace std;

typedef enum {
  I, 
  S,
	E,
	M,
} coh_state_t;

struct coh_dir_entry {
    coh_state_t state;
    bool sharers[N_SOCKETS];
    uint64_t owner;

    // coh_dir_entry(coh_state_t s, uint64_t ow, const bool* sh) : state(s), owner(ow) {
    //   for (int i = 0; i < N_SOCKETS; ++i) {
    //       sharers[i] = sh[i];
    //   }
    // }
};

class coh_directory
{
public:
  std::unordered_map<uint64_t, coh_dir_entry> directory;
  coh_directory() {}  
  int get_coh_owner(uint64_t addr, uint64_t cpu_id, bool isW);
  void update_block(uint64_t addr, uint64_t cpu_id, bool isW);
  void update_evict(uint64_t addr, uint64_t cpu_id, std::string caller_name);
  uint64_t send_inval_to_s0_caches(uint64_t addr);
  uint64_t reset_broken_block(uint64_t addr);
  void debug_print();

  
  uint64_t update_evict_calls=0;
  uint64_t n_block_transfers=0, n_block_transfers_S0=0;

  uint64_t n_inval_event=0, n_inval_msgs=0, n_invals_to_s0=0;
  
  //dbg counters
  uint64_t reset_broken_block_count=0;
  uint64_t evict_calls_from_LLC=0;
  uint64_t evict_calls_from_l1l2=0;
  uint64_t evict_but_update_evict_not_called=0;

};

#endif
