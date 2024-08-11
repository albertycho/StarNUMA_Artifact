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

#include "cache.h"

#include <algorithm>
#include <iterator>

#include "champsim.h"
#include "champsim_constants.h"
#include "util.h"
#include "vmem.h"
#include "coh_directory.h"
#include "icn_sim.h"
#include "migrator.h"

#ifndef SANITY_CHECK
#define NDEBUG
#endif


#define DBGPON 0

extern MIGRATOR mig0;

//dbg
bool debug_coherence = false;
uint64_t offending_lineaddr_cache_h = 1215854624;
uint64_t offending_addr = 77814695960;
uint64_t offending_insids[5]={458781,1535278,1508815,1508043,454621};
bool is_offending_ins(uint64_t insid){
  for(int i=0; i<5;i++){
    if(insid==offending_insids[i]){
      return true;
    }
  }
  return false;
}

extern VirtualMemory vmem;
extern uint8_t warmup_complete[NUM_CPUS];
extern std::array<CACHE*, NUM_CACHES> caches;
extern coh_directory coh_dir;

//extern uint64_t wait_till_next_TLB_shootdown[NUM_CPUS];
// using this to pace migration. rate limits migration more if any access gets blocked
extern uint64_t wait_till_next_TLB_shootdown;

void print_offending_block(uint64_t offending_addr, BLOCK fill_block, size_t set, size_t way, std::string c_name);
void print_offending_packet(PACKET handle_pkt, size_t set, size_t way, std::string c_name);
std::string type_itos(uint64_t typeI);

void CACHE::handle_fill()
{
  while (writes_available_this_cycle > 0) {
    auto fill_mshr = MSHR.begin();
    if (fill_mshr == std::end(MSHR) || fill_mshr->event_cycle > current_cycle)
      return;
    //  if (this->fill_level==6){
    //    std::cout<<"LLC MSHR addr: "<<fill_mshr->address<<", enqeue cycle: "<<fill_mshr->cycle_enqueued<<", event_cycle: "<<fill_mshr->event_cycle<<", cur_cycle: "<<current_cycle<<std::endl;
    //  }

    // find victim
    uint32_t set = get_set(fill_mshr->address);

    if(fill_mshr->block_transfer){
      uint32_t bt_way = get_way(fill_mshr->address, set);
      if(bt_way < NUM_WAY){ // was in cache but did block transfer
        
        impl_replacement_update_state(fill_mshr->cpu, set, bt_way, fill_mshr->address, fill_mshr->ip, 0, fill_mshr->type, 1);
        fill_mshr->data = block[set * NUM_WAY + bt_way].data;
        for (auto ret : fill_mshr->to_return) {ret->return_data(&(*fill_mshr));}
        MSHR.erase(fill_mshr);
        return;
      }
    }
    //otherwise just do regular fill

    auto set_begin = std::next(std::begin(block), set * NUM_WAY);
    auto set_end = std::next(set_begin, NUM_WAY);
    auto first_inv = std::find_if_not(set_begin, set_end, is_valid<BLOCK>());
    uint32_t way = std::distance(set_begin, first_inv);
    if (way == NUM_WAY)
      way = impl_replacement_find_victim(fill_mshr->cpu, fill_mshr->instr_id, set, &block.data()[set * NUM_WAY], fill_mshr->ip, fill_mshr->address,
                                         fill_mshr->type);

    bool success = filllike_miss(set, way, *fill_mshr);
    if (!success)
      return;

    if (way != NUM_WAY) {
      // update processed packets
      fill_mshr->data = block[set * NUM_WAY + way].data;

      for (auto ret : fill_mshr->to_return)
        ret->return_data(&(*fill_mshr));
    }

    MSHR.erase(fill_mshr);
    writes_available_this_cycle--;
  }
}

void CACHE::handle_writeback()
{
  while (writes_available_this_cycle > 0) {
    if (!WQ.has_ready())
      return;

    // handle the oldest entry
    PACKET& handle_pkt = WQ.front();

    // access cache
    uint32_t set = get_set(handle_pkt.address);
    uint32_t way = get_way(handle_pkt.address, set);

    BLOCK& fill_block = block[set * NUM_WAY + way];

    if (way < NUM_WAY) // HIT
    {
      impl_replacement_update_state(handle_pkt.cpu, set, way, fill_block.address, handle_pkt.ip, 0, handle_pkt.type, 1);

      // COLLECT STATS
      sim_hit[handle_pkt.cpu][handle_pkt.type]++;
      sim_access[handle_pkt.cpu][handle_pkt.type]++;

      // mark dirty
      fill_block.dirty = 1;
    } else // MISS
    {
      bool success;
      if (handle_pkt.type == RFO && handle_pkt.to_return.empty()) {
        success = readlike_miss(handle_pkt);
      } else {
        // find victim
        auto set_begin = std::next(std::begin(block), set * NUM_WAY);
        auto set_end = std::next(set_begin, NUM_WAY);
        auto first_inv = std::find_if_not(set_begin, set_end, is_valid<BLOCK>());
        way = std::distance(set_begin, first_inv);
        if (way == NUM_WAY)
          way = impl_replacement_find_victim(handle_pkt.cpu, handle_pkt.instr_id, set, &block.data()[set * NUM_WAY], handle_pkt.ip, handle_pkt.address,
                                             handle_pkt.type);

        success = filllike_miss(set, way, handle_pkt);
      }

      if (!success)
        return;
    }

    // remove this entry from WQ
    writes_available_this_cycle--;
    WQ.pop_front();
  }
}

void CACHE::handle_read()
{
  while (reads_available_this_cycle > 0) {

    if (!RQ.has_ready())
      return;

    // handle the oldest entry
    PACKET& handle_pkt = RQ.front();

    //dbg
    // if(NAME=="LLC"){
    //   cout<<"RQ has ready, RQ front insid: "<<handle_pkt.instr_id<<", RQ occupancy: "<<RQ.occupancy()<<endl;
    // }

    // A (hopefully temporary) hack to know whether to send the evicted paddr or
    // vaddr to the prefetcher
    ever_seen_data |= (handle_pkt.v_address != handle_pkt.ip);

    uint32_t set = get_set(handle_pkt.address);
    uint32_t way = get_way(handle_pkt.address, set);

    ///DBG print TODO - remove
    if(is_offending_ins(handle_pkt.instr_id)){
      cout<<"offending ins "<<handle_pkt.instr_id<<" in handle_read in "<<NAME<<endl;
      cout<<"is hit?: "<<(way<NUM_WAY)<<", block_transfer?: "<<handle_pkt.block_transfer<<endl;
    }


    if (way < NUM_WAY) // HIT
    {
      if(handle_pkt.block_transfer){
        bool success = readlike_miss(handle_pkt);
        if (!success){return;}
        n_hit_but_block_transfer++;
      }
      else{
        readlike_hit(set, way, handle_pkt);
      }
    } else {
      bool success = readlike_miss(handle_pkt);
      if (!success)
        return;
    }

    if(is_offending_ins(handle_pkt.instr_id)){
      cout<<"RQ entry successfully read and calling RQ.pop_front() for offending ins "<<handle_pkt.instr_id<<" in handle_read in "<<NAME<<endl;
    }

    // remove this entry from RQ
    RQ.pop_front();
    reads_available_this_cycle--;
  }
}

void CACHE::handle_prefetch()
{
  while (reads_available_this_cycle > 0) {
    if (!PQ.has_ready())
      return;

    // handle the oldest entry
    PACKET& handle_pkt = PQ.front();

    uint32_t set = get_set(handle_pkt.address);
    uint32_t way = get_way(handle_pkt.address, set);

    if (way < NUM_WAY) // HIT
    {
      readlike_hit(set, way, handle_pkt);
    } else {
      bool success = readlike_miss(handle_pkt);
      if (!success)
        return;
    }

    // remove this entry from PQ
    PQ.pop_front();
    reads_available_this_cycle--;
  }
}

void CACHE::readlike_hit(std::size_t set, std::size_t way, PACKET& handle_pkt)
{
  DP(if (warmup_complete[handle_pkt.cpu]) {
    std::cout << "[" << NAME << "] " << __func__ << " hit";
    std::cout << " instr_id: " << handle_pkt.instr_id << " address: " << std::hex << (handle_pkt.address >> OFFSET_BITS);
    std::cout << " full_addr: " << handle_pkt.address;
    std::cout << " full_v_addr: " << handle_pkt.v_address << std::dec;
    std::cout << " type: " << +handle_pkt.type;
    std::cout << " cycle: " << current_cycle << std::endl;
  });

  BLOCK& hit_block = block[set * NUM_WAY + way];

  handle_pkt.data = hit_block.data;

  // update prefetcher on load instruction
  if (should_activate_prefetcher(handle_pkt.type) && handle_pkt.pf_origin_level < fill_level) {
    cpu = handle_pkt.cpu;
    uint64_t pf_base_addr = (virtual_prefetch ? handle_pkt.v_address : handle_pkt.address) & ~bitmask(match_offset_bits ? 0 : OFFSET_BITS);
    handle_pkt.pf_metadata = impl_prefetcher_cache_operate(pf_base_addr, handle_pkt.ip, 1, handle_pkt.type, handle_pkt.pf_metadata);
  }

  // update replacement policy
  impl_replacement_update_state(handle_pkt.cpu, set, way, hit_block.address, handle_pkt.ip, 0, handle_pkt.type, 1);

  // COLLECT STATS
  sim_hit[handle_pkt.cpu][handle_pkt.type]++;
  sim_access[handle_pkt.cpu][handle_pkt.type]++;

  for (auto ret : handle_pkt.to_return)
    ret->return_data(&handle_pkt);

  // update prefetch stats and reset prefetch bit
  if (hit_block.prefetch) {
    pf_useful++;
    hit_block.prefetch = 0;
  }
}

bool CACHE::hit_but_block_transfer(PACKET& handle_pkt){
  //allocate MSHR to deal with to-return?
  //  don't need duplicate check - should be no outstanding mshr for this, since it's already in cache
  //
  //try just calling readlike_miss..
  return true;
}

bool CACHE::readlike_miss(PACKET& handle_pkt)
{
  DP(if (warmup_complete[handle_pkt.cpu]) {
    std::cout << "[" << NAME << "] " << __func__ << " miss";
    std::cout << " instr_id: " << handle_pkt.instr_id << " address: " << std::hex << (handle_pkt.address >> OFFSET_BITS);
    std::cout << " full_addr: " << handle_pkt.address;
    std::cout << " full_v_addr: " << handle_pkt.v_address << std::dec;
    std::cout << " type: " << +handle_pkt.type;
    std::cout << " cycle: " << current_cycle << std::endl;
  });

  if(is_offending_ins(handle_pkt.instr_id)){
    cout<<"offending ins "<<handle_pkt.instr_id<<" in readlike_miss in "<<NAME<<endl;
  }

  // check mshr
  auto mshr_entry = std::find_if(MSHR.begin(), MSHR.end(), eq_addr<PACKET>(handle_pkt.address, OFFSET_BITS));
  bool mshr_full = (MSHR.size() == MSHR_SIZE);

  if (mshr_entry != MSHR.end()) // miss already inflight
  {
    // update fill location
    mshr_entry->fill_level = std::min(mshr_entry->fill_level, handle_pkt.fill_level);

    packet_dep_merge(mshr_entry->lq_index_depend_on_me, handle_pkt.lq_index_depend_on_me);
    packet_dep_merge(mshr_entry->sq_index_depend_on_me, handle_pkt.sq_index_depend_on_me);
    packet_dep_merge(mshr_entry->instr_depend_on_me, handle_pkt.instr_depend_on_me);
    packet_dep_merge(mshr_entry->to_return, handle_pkt.to_return);

    if (mshr_entry->type == PREFETCH && handle_pkt.type != PREFETCH) {
      // Mark the prefetch as useful
      if (mshr_entry->pf_origin_level == fill_level)
        pf_useful++;

      uint64_t prior_event_cycle = mshr_entry->event_cycle;
      *mshr_entry = handle_pkt;

      // in case request is already returned, we should keep event_cycle
      mshr_entry->event_cycle = prior_event_cycle;
    }
  } else {
    if (mshr_full){  // not enough MSHR resource
	
#if DBGPON
      std::cout << NAME << " MSHR FULL!" <<std::endl;
      if(is_offending_ins(handle_pkt.instr_id)){
        cout<<"offending ins "<<handle_pkt.instr_id<<" in "<<NAME<<endl;
      }
#endif
      return false; // TODO should we allow prefetches anyway if they will not
                    // be filled to this level?
    }

    bool is_read = prefetch_as_load || (handle_pkt.type != PREFETCH);

    // check to make sure the lower level queue has room for this read miss
    int queue_type = (is_read) ? 1 : 3;
    //if(this->fill_level == 6){
    if(this->NAME == "LLC"){
      //LLC to ICN_SIM. Need to pass source node info. Shove it into LSBs of address
      uint64_t addr_w_src = handle_pkt.address >> LOG2_BLOCK_SIZE;
      addr_w_src = addr_w_src << LOG2_BLOCK_SIZE;
      addr_w_src = addr_w_src | (handle_pkt.cpu / NUM_CPUS);
      //handle_pkt.address=addr_w_src;
      //if (lower_level->get_occupancy(queue_type, addr_w_src) == lower_level->get_size(queue_type, addr_w_src))
      if (((ICN_SIM *)lower_level)->get_occupancy_icn(queue_type, addr_w_src, handle_pkt) == ((ICN_SIM *)lower_level)->get_size_icn(queue_type, addr_w_src, handle_pkt)){
        return false;  
      }
      uint64_t vpageN = handle_pkt.v_address >>LOG2_PAGE_SIZE;
      for(int i=0; i<N_SOCKETS;i++){
        if(vpageN == mig0.mig_in_flight[i]){
          mig0.n_denied_access_to_mirating_page++;
	        consecutive_blocks_from_migration++;
	        blocking_packet=handle_pkt;
          // pace migration so that we don't get prolonged periods of blocking that lead to deadlock
          wait_till_next_TLB_shootdown=6000;

          if(consecutive_blocks_from_migration>1000000){
            //let the access through for 
            wait_till_next_TLB_shootdown=500000; //give enough delay for next migration to give pipeline to progress
            cout<<"Allowing access after "<<consecutive_blocks_from_migration<<"consecutive stalls at LLC"<<endl;
          }
          else{
            return false;
          }
        }
      }
    }
    else{
      if (lower_level->get_occupancy(queue_type, handle_pkt.address) == lower_level->get_size(queue_type, handle_pkt.address))
        return false;  
    }

	consecutive_blocks_from_migration=0;

    // Allocate an MSHR
    if (handle_pkt.fill_level <= fill_level) {
      auto it = MSHR.insert(std::end(MSHR), handle_pkt);
      it->cycle_enqueued = current_cycle;
      it->event_cycle = std::numeric_limits<uint64_t>::max();
      if(is_offending_ins(handle_pkt.instr_id)){
        cout<<"MSHR allocated for offending ins "<<handle_pkt.instr_id<<" in "<<NAME<<endl;
      }
    }

    if (handle_pkt.fill_level <= fill_level)
      handle_pkt.to_return = {this};
    else
      handle_pkt.to_return.clear();

    if (!is_read)
      lower_level->add_pq(&handle_pkt);
    else{
      int add_rq_ret = lower_level->add_rq(&handle_pkt);
      if(is_offending_ins(handle_pkt.instr_id)){
        cout<<"add rq to next level returned "<<add_rq_ret<<" in "<<NAME<<" for "<< handle_pkt.instr_id<<endl;
      }
      if(add_rq_ret==-2){ // code should not get here
        cout<<NAME<<" : lower_level->add_rq failed"<<endl;
        cout<<" Packet insid: "<<handle_pkt.instr_id<<endl;
        cout<<" Packet  Addr: "<<handle_pkt.address<<endl;
        cout<<" Packet VAddr: "<<handle_pkt.v_address<<endl;
        cout<<"  ll getsize: "<<lower_level->get_size(queue_type, handle_pkt.address);
        cout<<"  ll getoccu: "<<lower_level->get_occupancy(queue_type, handle_pkt.address);
        cout<<"  llName: "<< lower_level->get_name()<<endl;
        //DBG check check fucntion
        //if(this->fill_level == 6){
        if(this->NAME == "LLC"){
          //LLC to ICN_SIM. Need to pass source node info. Shove it into LSBs of address
          uint64_t addr_w_src = handle_pkt.address >> LOG2_BLOCK_SIZE;
          addr_w_src = addr_w_src << LOG2_BLOCK_SIZE;
          addr_w_src = addr_w_src | handle_pkt.cpu;
          //handle_pkt.address=addr_w_src;
          cout<<"  src: "<<handle_pkt.cpu<<endl;
          cout<<"  ll occupancy with addr_w_src: "<<((ICN_SIM *)lower_level)->get_occupancy_icn(queue_type, addr_w_src, handle_pkt); 
          cout<<"  ll get_size with addr_w_src: "<<((ICN_SIM *)lower_level)->get_size_icn(queue_type, addr_w_src, handle_pkt)<<endl;
        }
     
      }
    }

    if(NAME == "LLC" && handle_pkt.type == LOAD && handle_pkt.v_address != 0)
    {
        uint64_t outstanding_accesses = MSHR.size();
        if(outstanding_accesses>299) outstanding_accesses=299;
        mlp_hist[handle_pkt.cpu][outstanding_accesses]++;
    }
  }

  // update prefetcher on load instructions and prefetches from upper levels
  if (should_activate_prefetcher(handle_pkt.type) && handle_pkt.pf_origin_level < fill_level) {
    cpu = handle_pkt.cpu;
    uint64_t pf_base_addr = (virtual_prefetch ? handle_pkt.v_address : handle_pkt.address) & ~bitmask(match_offset_bits ? 0 : OFFSET_BITS);
    handle_pkt.pf_metadata = impl_prefetcher_cache_operate(pf_base_addr, handle_pkt.ip, 0, handle_pkt.type, handle_pkt.pf_metadata);
  }

  return true;
}

bool CACHE::filllike_miss(std::size_t set, std::size_t way, PACKET& handle_pkt)
{
  DP(if (warmup_complete[handle_pkt.cpu]) {
    std::cout << "[" << NAME << "] " << __func__ << " miss";
    std::cout << " instr_id: " << handle_pkt.instr_id << " address: " << std::hex << (handle_pkt.address >> OFFSET_BITS);
    std::cout << " full_addr: " << handle_pkt.address;
    std::cout << " full_v_addr: " << handle_pkt.v_address << std::dec;
    std::cout << " type: " << +handle_pkt.type;
    std::cout << " cycle: " << current_cycle << std::endl;
  });

  bool bypass = (way == NUM_WAY);
#ifndef LLC_BYPASS
  assert(!bypass);
#endif
  assert(handle_pkt.type != WRITEBACK || !bypass);

  BLOCK& fill_block = block[set * NUM_WAY + way];
  bool evicting_dirty = !bypass && (lower_level != NULL) && fill_block.dirty;
  uint64_t evicting_address = 0;

  if(handle_pkt.type==TRANSLATION){
    //TLB lookup only simulated by socket0(timing cores)
    // update coherence for it here. Needed only to supress assertion when evicting
    coh_dir.update_block(handle_pkt.address, cpu, false);
  }
  if(handle_pkt.type==PREFETCH){
    //TODO - prefetch should be handled properly
    // i.e. at issue time. putting this here to move on for now
    coh_dir.update_block(handle_pkt.address, cpu, false);
  }
  //TODO - following will probably remove errors from WB
  if(handle_pkt.type==WRITEBACK){
    coh_dir.update_block(handle_pkt.address, cpu, true);
  }


  //DBG
  if(debug_coherence){
    // if(set==offendingSet && way==offendingWay){
    //   print_offending_packet(handle_pkt, set, way, NAME);
    // }
    if((handle_pkt.address>>LOG2_BLOCK_SIZE)==offending_lineaddr_cache_h){
      print_offending_packet(handle_pkt, set, way, NAME);
    }
  }

  if (!bypass) {
#if  ENABLE_COHERENCE
    bool eviction_happens = fill_block.valid && (lower_level != NULL);
    bool is_d_cache = false;
    if (NAME.find("L1D") != std::string::npos){is_d_cache=true;}
    if (NAME.find("L2C") != std::string::npos){is_d_cache=true;}
    if (NAME.find("LLC") != std::string::npos){is_d_cache=true;}
    if(eviction_happens && is_d_cache){ 
      // block gets evicted (dirty or not), 
      //  check if there is no other copy in the socket, 
      //  then update directory
      bool is_not_in_socket0=true;
      for (CACHE* cache : caches) {
        bool is_d_cache_iter = false;
        if (cache->NAME.find("L1D") != std::string::npos){is_d_cache_iter=true;}
        if (cache->NAME.find("L2C") != std::string::npos){is_d_cache_iter=true;}
        if (cache->NAME.find("LLC") != std::string::npos){is_d_cache_iter=true;}
        if (NAME==cache->NAME){is_d_cache_iter=false;} //don't check itself
        if(is_d_cache_iter){
          bool probe_hit = cache->probe_entry(fill_block.address);
          if(probe_hit){is_not_in_socket0=false;}
        }
      }
      if(is_not_in_socket0){
        //TODO udpate directory for eviction
        //DBG
        if(debug_coherence){
          print_offending_block(offending_addr, fill_block, set, way, NAME);
        }

        if(!((NAME!="LLC") && (evicting_dirty))){ 
          //if not LLC and evicting dirty, block will go to uppper level
          //In all other cases, block is evicted out of the socket
          coh_dir.update_evict(fill_block.address, cpu, NAME);
        }
        //coh_dir.update_evict(fill_block.address, cpu, NAME);
      }
      else{ //DBG - no evict update- cuz block still in socket 0 in some other cache
        coh_dir.evict_but_update_evict_not_called++;
        if(NAME.find("LLC") != std::string::npos) coh_dir.evict_calls_from_LLC++;
        else coh_dir.evict_calls_from_l1l2++;
      }

    }
#endif
    if (evicting_dirty) {
      PACKET writeback_packet;

      writeback_packet.fill_level = lower_level->fill_level;
      writeback_packet.cpu = handle_pkt.cpu;
      writeback_packet.address = fill_block.address;
      writeback_packet.data = fill_block.data;
      writeback_packet.instr_id = handle_pkt.instr_id;
      writeback_packet.ip = 0;
      writeback_packet.type = WRITEBACK;
      
      ////DBG TODO remove
      if(debug_coherence){
        if((writeback_packet.address>>LOG2_BLOCK_SIZE)==offending_lineaddr_cache_h){
          cout<<"dirty WB offending block"<<endl;
          print_offending_packet(writeback_packet, 0,0, NAME);
        }
      }


      auto result = lower_level->add_wq(&writeback_packet);
      if (result == -2)
        return false;
    }

    if (ever_seen_data)
      evicting_address = fill_block.address & ~bitmask(match_offset_bits ? 0 : OFFSET_BITS);
    else
      evicting_address = fill_block.v_address & ~bitmask(match_offset_bits ? 0 : OFFSET_BITS);

    if (fill_block.prefetch)
      pf_useless++;

    if (handle_pkt.type == PREFETCH)
      pf_fill++;

    fill_block.valid = true;
    fill_block.prefetch = (handle_pkt.type == PREFETCH && handle_pkt.pf_origin_level == fill_level);
    fill_block.dirty = (handle_pkt.type == WRITEBACK || (handle_pkt.type == RFO && handle_pkt.to_return.empty()));
    fill_block.address = handle_pkt.address;
    fill_block.v_address = handle_pkt.v_address;
    fill_block.data = handle_pkt.data;
    fill_block.ip = handle_pkt.ip;
    fill_block.cpu = handle_pkt.cpu;
    fill_block.instr_id = handle_pkt.instr_id;
  }

  uint64_t miss_lat = current_cycle - handle_pkt.cycle_enqueued;
  if (warmup_complete[handle_pkt.cpu] && (handle_pkt.cycle_enqueued != 0)){
    total_miss_latency[handle_pkt.cpu] += miss_lat;
  	if(this->fill_level == 6){
      int hist_index = miss_lat / 24; // divide by 2.4 to convert to ns, then by 10 for histogram(10ns per bucket)
	    if(hist_index>99){ 
        hist_index=99;
      }
      lat_hist[handle_pkt.cpu][hist_index]++;
      
    }

  }
  

  // update prefetcher
  cpu = handle_pkt.cpu;
  handle_pkt.pf_metadata =
      impl_prefetcher_cache_fill((virtual_prefetch ? handle_pkt.v_address : handle_pkt.address) & ~bitmask(match_offset_bits ? 0 : OFFSET_BITS), set, way,
                                 handle_pkt.type == PREFETCH, evicting_address, handle_pkt.pf_metadata);

  // update replacement policy
  impl_replacement_update_state(handle_pkt.cpu, set, way, handle_pkt.address, handle_pkt.ip, 0, handle_pkt.type, 0);

  // COLLECT STATS
  sim_miss[handle_pkt.cpu][handle_pkt.type]++;
  sim_access[handle_pkt.cpu][handle_pkt.type]++;

  return true;
}

void CACHE::operate()
{
  operate_writes();
  operate_reads();

  impl_prefetcher_cycle_operate();
}

void CACHE::operate_writes()
{
  // perform all writes
  writes_available_this_cycle = MAX_WRITE;
  handle_fill();
  handle_writeback();

  WQ.operate();
}

void CACHE::operate_reads()
{
  // perform all reads
  reads_available_this_cycle = MAX_READ;
  handle_read();
  va_translate_prefetches();
  handle_prefetch();

  RQ.operate();
  PQ.operate();
  VAPQ.operate();
}

uint32_t CACHE::get_set(uint64_t address) { return ((address >> OFFSET_BITS) & bitmask(lg2(NUM_SET))); }

uint32_t CACHE::get_way(uint64_t address, uint32_t set)
{
  auto begin = std::next(block.begin(), set * NUM_WAY);
  auto end = std::next(begin, NUM_WAY);
  return std::distance(begin, std::find_if(begin, end, eq_addr<BLOCK>(address, OFFSET_BITS)));
}

int CACHE::invalidate_entry(uint64_t inval_addr)
{
  uint32_t set = get_set(inval_addr);
  uint32_t way = get_way(inval_addr, set);

  if (way < NUM_WAY){
    block[set * NUM_WAY + way].valid = 0;
    block[set * NUM_WAY + way].dirty=0;
  }
  else{
    return -1;
  }
  return way;
}

bool CACHE::probe_entry(uint64_t addr)
{
  uint32_t set = get_set(addr);
  uint32_t way = get_way(addr, set);

  if (way < NUM_WAY){
    return true;
  }
  return false;  
}

int CACHE::add_rq(PACKET* packet)
{
  
  if(is_offending_ins(packet->instr_id)){
    cout<<"offending ins "<<packet->instr_id<<" in add_rq in "<<NAME<<endl;
  }

  assert(packet->address != 0);
  // //WA, see if things break further..
  // if(packet->address == 0){
	//   std::cout<<"cache.cc Warning - address ==0 detected"<<std::endl;
  //   packet->address=100;
  // }
  //else{
  //    std::cout<<"Address: "<<packet->address<<std::endl;
  //}
  RQ_ACCESS++;

  DP(if (warmup_complete[packet->cpu]) {
    std::cout << "[" << NAME << "_RQ] " << __func__ << " instr_id: " << packet->instr_id << " address: " << std::hex << (packet->address >> OFFSET_BITS);
    std::cout << " full_addr: " << packet->address << " v_address: " << packet->v_address << std::dec << " type: " << +packet->type
              << " occupancy: " << RQ.occupancy();
  })

  // check for the latest writebacks in the write queue
  champsim::delay_queue<PACKET>::iterator found_wq = std::find_if(WQ.begin(), WQ.end(), eq_addr<PACKET>(packet->address, match_offset_bits ? 0 : OFFSET_BITS));

  if (found_wq != WQ.end()) {

    DP(if (warmup_complete[packet->cpu]) std::cout << " MERGED_WQ" << std::endl;)

    packet->data = found_wq->data;
    for (auto ret : packet->to_return)
      ret->return_data(packet);

    WQ_FORWARD++;
    return -1;
  }

  // check for duplicates in the read queue
  auto found_rq = std::find_if(RQ.begin(), RQ.end(), eq_addr<PACKET>(packet->address, OFFSET_BITS));
  if (found_rq != RQ.end()) {

    DP(if (warmup_complete[packet->cpu]) std::cout << " MERGED_RQ" << std::endl;)

    packet_dep_merge(found_rq->lq_index_depend_on_me, packet->lq_index_depend_on_me);
    packet_dep_merge(found_rq->sq_index_depend_on_me, packet->sq_index_depend_on_me);
    packet_dep_merge(found_rq->instr_depend_on_me, packet->instr_depend_on_me);
    packet_dep_merge(found_rq->to_return, packet->to_return);

    RQ_MERGED++;

    return 0; // merged index
  }

  // check occupancy
  if (RQ.full()) {
    RQ_FULL++;

    if(is_offending_ins(packet->instr_id)){
      cout<<"offending ins "<<packet->instr_id<<" RQ full in "<<NAME<<endl;
    }

    DP(if (warmup_complete[packet->cpu]) std::cout << " FULL" << std::endl;)

    return -2; // cannot handle this request
  }

  // if there is no duplicate, add it to RQ
  if (warmup_complete[cpu])
    RQ.push_back(*packet);
  else
    RQ.push_back_ready(*packet);
      
  if(is_offending_ins(packet->instr_id)){
    cout<<"offending ins "<<packet->instr_id<<" added to RQ in "<<NAME;
    cout<<", RQ occpuancy : "<<RQ.occupancy()<<"/"<<RQ.size()<<endl;
  }


  DP(if (warmup_complete[packet->cpu]) std::cout << " ADDED" << std::endl;)

  RQ_TO_CACHE++;
  return RQ.occupancy();
}

int CACHE::add_wq(PACKET* packet)
{
  WQ_ACCESS++;

  DP(if (warmup_complete[packet->cpu]) {
    std::cout << "[" << NAME << "_WQ] " << __func__ << " instr_id: " << packet->instr_id << " address: " << std::hex << (packet->address >> OFFSET_BITS);
    std::cout << " full_addr: " << packet->address << " v_address: " << packet->v_address << std::dec << " type: " << +packet->type
              << " occupancy: " << RQ.occupancy();
  })

  // check for duplicates in the write queue
  champsim::delay_queue<PACKET>::iterator found_wq = std::find_if(WQ.begin(), WQ.end(), eq_addr<PACKET>(packet->address, match_offset_bits ? 0 : OFFSET_BITS));

  if (found_wq != WQ.end()) {

    DP(if (warmup_complete[packet->cpu]) std::cout << " MERGED" << std::endl;)

    WQ_MERGED++;
    return 0; // merged index
  }

  // Check for room in the queue
  if (WQ.full()) {
    DP(if (warmup_complete[packet->cpu]) std::cout << " FULL" << std::endl;)

    ++WQ_FULL;
    return -2;
  }

  // if there is no duplicate, add it to the write queue
  if (warmup_complete[cpu])
    WQ.push_back(*packet);
  else
    WQ.push_back_ready(*packet);

  DP(if (warmup_complete[packet->cpu]) std::cout << " ADDED" << std::endl;)

  WQ_TO_CACHE++;
  WQ_ACCESS++;

  return WQ.occupancy();
}

int CACHE::prefetch_line(uint64_t pf_addr, bool fill_this_level, uint32_t prefetch_metadata)
{
  pf_requested++;

  PACKET pf_packet;
  pf_packet.type = PREFETCH;
  pf_packet.fill_level = (fill_this_level ? fill_level : lower_level->fill_level);
  pf_packet.pf_origin_level = fill_level;
  pf_packet.pf_metadata = prefetch_metadata;
  pf_packet.cpu = cpu;
  pf_packet.address = pf_addr;
  pf_packet.v_address = virtual_prefetch ? pf_addr : 0;

  if (virtual_prefetch) {
    if (!VAPQ.full()) {
      VAPQ.push_back(pf_packet);
      return 1;
    }
  } else {
    int result = add_pq(&pf_packet);
    if (result != -2) {
      if (result > 0)
        pf_issued++;
      return 1;
    }
  }

  return 0;
}

int CACHE::prefetch_line(uint64_t ip, uint64_t base_addr, uint64_t pf_addr, bool fill_this_level, uint32_t prefetch_metadata)
{
  static bool deprecate_printed = false;
  if (!deprecate_printed) {
    std::cout << "WARNING: The extended signature CACHE::prefetch_line(ip, "
                 "base_addr, pf_addr, fill_this_level, prefetch_metadata) is "
                 "deprecated."
              << std::endl;
    std::cout << "WARNING: Use CACHE::prefetch_line(pf_addr, fill_this_level, "
                 "prefetch_metadata) instead."
              << std::endl;
    deprecate_printed = true;
  }
  return prefetch_line(pf_addr, fill_this_level, prefetch_metadata);
}

void CACHE::va_translate_prefetches()
{
  // TEMPORARY SOLUTION: mark prefetches as translated after a fixed latency
  if (VAPQ.has_ready()) {
    VAPQ.front().address = vmem.va_to_pa(cpu, VAPQ.front().v_address).first;

    // move the translated prefetch over to the regular PQ
    int result = add_pq(&VAPQ.front());

    // remove the prefetch from the VAPQ
    if (result != -2)
      VAPQ.pop_front();

    if (result > 0)
      pf_issued++;
  }
}

int CACHE::add_pq(PACKET* packet)
{
  assert(packet->address != 0);
  PQ_ACCESS++;

  DP(if (warmup_complete[packet->cpu]) {
    std::cout << "[" << NAME << "_WQ] " << __func__ << " instr_id: " << packet->instr_id << " address: " << std::hex << (packet->address >> OFFSET_BITS);
    std::cout << " full_addr: " << packet->address << " v_address: " << packet->v_address << std::dec << " type: " << +packet->type
              << " occupancy: " << RQ.occupancy();
  })

  // check for the latest wirtebacks in the write queue
  champsim::delay_queue<PACKET>::iterator found_wq = std::find_if(WQ.begin(), WQ.end(), eq_addr<PACKET>(packet->address, match_offset_bits ? 0 : OFFSET_BITS));

  if (found_wq != WQ.end()) {

    DP(if (warmup_complete[packet->cpu]) std::cout << " MERGED_WQ" << std::endl;)

    packet->data = found_wq->data;
    for (auto ret : packet->to_return)
      ret->return_data(packet);

    WQ_FORWARD++;
    return -1;
  }

  // check for duplicates in the PQ
  auto found = std::find_if(PQ.begin(), PQ.end(), eq_addr<PACKET>(packet->address, OFFSET_BITS));
  if (found != PQ.end()) {
    DP(if (warmup_complete[packet->cpu]) std::cout << " MERGED_PQ" << std::endl;)

    found->fill_level = std::min(found->fill_level, packet->fill_level);
    packet_dep_merge(found->to_return, packet->to_return);

    PQ_MERGED++;
    return 0;
  }

  // check occupancy
  if (PQ.full()) {

    DP(if (warmup_complete[packet->cpu]) std::cout << " FULL" << std::endl;)

    PQ_FULL++;
    return -2; // cannot handle this request
  }

  // if there is no duplicate, add it to PQ
  if (warmup_complete[cpu])
    PQ.push_back(*packet);
  else
    PQ.push_back_ready(*packet);

  DP(if (warmup_complete[packet->cpu]) std::cout << " ADDED" << std::endl;)

  PQ_TO_CACHE++;
  return PQ.occupancy();
}

void CACHE::return_data(PACKET* packet)
{
  // check MSHR information
  auto mshr_entry = std::find_if(MSHR.begin(), MSHR.end(), eq_addr<PACKET>(packet->address, OFFSET_BITS));
  auto first_unreturned = std::find_if(MSHR.begin(), MSHR.end(), [](auto x) { return x.event_cycle == std::numeric_limits<uint64_t>::max(); });

  // sanity check
  if (mshr_entry == MSHR.end()) {
    std::cerr << "[" << NAME << "_MSHR] " << __func__ << " instr_id: " << packet->instr_id << " cannot find a matching entry!";
    std::cerr << " address: " << std::hex << packet->address;
    std::cerr << " v_address: " << packet->v_address;
    std::cerr << " address: " << (packet->address >> OFFSET_BITS) << std::dec;
    std::cerr << " event: " << packet->event_cycle << " current: " << current_cycle << std::endl;
    assert(0);
  }

  // MSHR holds the most updated information about this request
  mshr_entry->data = packet->data;
  mshr_entry->pf_metadata = packet->pf_metadata;
  mshr_entry->event_cycle = current_cycle + (warmup_complete[cpu] ? FILL_LATENCY : 0);
  //if(warmup_complete[cpu]){
  //  if(this->fill_level==6){ //LLC CXL WA. TODO REMOVE!!!
  //    mshr_entry->event_cycle = current_cycle+80;
  //    //std::cout<<"adding 30ns from mem to LLC"<<std::endl;
  //  }
  //}

  DP(if (warmup_complete[packet->cpu]) {
    std::cout << "[" << NAME << "_MSHR] " << __func__ << " instr_id: " << mshr_entry->instr_id;
    std::cout << " address: " << std::hex << (mshr_entry->address >> OFFSET_BITS) << " full_addr: " << mshr_entry->address;
    std::cout << " data: " << mshr_entry->data << std::dec;
    std::cout << " index: " << std::distance(MSHR.begin(), mshr_entry) << " occupancy: " << get_occupancy(0, 0);
    std::cout << " event: " << mshr_entry->event_cycle << " current: " << current_cycle << std::endl;
  });

  // Order this entry after previously-returned entries, but before non-returned
  // entries
  std::iter_swap(mshr_entry, first_unreturned);
}

uint32_t CACHE::get_occupancy(uint8_t queue_type, uint64_t address)
{
  if (queue_type == 0)
    return std::count_if(MSHR.begin(), MSHR.end(), is_valid<PACKET>());
  else if (queue_type == 1)
    return RQ.occupancy();
  else if (queue_type == 2)
    return WQ.occupancy();
  else if (queue_type == 3)
    return PQ.occupancy();

  return 0;
}

uint32_t CACHE::get_size(uint8_t queue_type, uint64_t address)
{
  if (queue_type == 0)
    return MSHR_SIZE;
  else if (queue_type == 1)
    return RQ.size();
  else if (queue_type == 2)
    return WQ.size();
  else if (queue_type == 3)
    return PQ.size();

  return 0;
}

bool CACHE::should_activate_prefetcher(int type) { return (1 << static_cast<int>(type)) & pref_activate_mask; }

void CACHE::print_deadlock()
{
  if (!std::empty(MSHR)) {
    std::cout << NAME << " MSHR Entry" << std::endl;
    std::size_t j = 0;
    for (PACKET entry : MSHR) {
      std::cout << "[" << NAME << " MSHR] entry: " << j++ << " instr_id: " << entry.instr_id;
      //std::cout << " address: " << std::hex << (entry.address >> LOG2_BLOCK_SIZE) << " full_addr: " << entry.address << std::dec << " type: " << +entry.type;
      std::cout << " address: " << std::hex << (entry.address >> LOG2_BLOCK_SIZE) << " full_addr: " << entry.address << std::dec << " type: " << type_itos(entry.type);
      //std::cout << " fill_level: " << +entry.fill_level << " event_cycle: " << entry.event_cycle << std::endl;
      std::cout << " fill_level: " << +entry.fill_level << " event_cycle: " << entry.event_cycle<<" cycle_enqueud: "<<entry.cycle_enqueued<<" timeInMSHR: "<<current_cycle-entry.cycle_enqueued<<endl;
    }
  } else {
    std::cout << NAME << " MSHR empty" << std::endl;
  }
  if(NAME=="LLC"){
  	std::cout << NAME << " consecutive cycles in block due to access to migrating page: "<<  consecutive_blocks_from_migration<<std::endl;
	std::cout << " Blocking Packet vaddr: " << blocking_packet.v_address <<", paddr: "<< blocking_packet.address <<", from cpu"<< blocking_packet.cpu<<std::endl;
  }
}

uint64_t CACHE::get_warm_status(){
  uint64_t num_blocks = NUM_WAY*NUM_SET;
  uint64_t valid_blocks = 0;
  for(uint32_t i=0;i<NUM_SET;i++){
    for(uint32_t j=0;j<NUM_WAY;j++){
      if(block[i * NUM_WAY + j].valid){valid_blocks++;}
    }
  }
  uint64_t warm_rate = valid_blocks*100 / num_blocks;
  std::cout<<NAME<<" is "<<warm_rate<<"% warm ("<<valid_blocks<<"/"<<num_blocks<<")" <<endl;
  return warm_rate;
}


void print_offending_block(uint64_t offending_addr, BLOCK fill_block, size_t set, size_t way, std::string c_name){
  //if(fill_block.address==offending_addr){
  if((fill_block.address>>LOG2_BLOCK_SIZE)==(offending_addr>>LOG2_BLOCK_SIZE)){
    cout<<"fill block v_addr: "<<fill_block.v_address<<endl;
    cout<<"fillblock valid? : "<<fill_block.valid<<endl;
    cout<<"fillblock_dirty? : "<<fill_block.dirty<<endl;
    cout<<"fillblock prefetch? : "<<fill_block.prefetch<<endl;
    cout<<"fillblock inst_id : "<<fill_block.instr_id<<endl;
    cout<<"fillblock PA: "<<fill_block.address<<endl;
    cout<<"offending set: "<<set<<endl;
    cout<<"offending way: "<<way<<endl;
    cout<<"cache name: "<<c_name<<endl;
  }

}

void print_offending_packet(PACKET handle_pkt, size_t set, size_t way, std::string c_name){
  std::cout << "[" << c_name << "] " << __func__ << " miss";
  std::cout << " instr_id: " << handle_pkt.instr_id << " address: " << std::dec << (handle_pkt.address >> LOG2_BLOCK_SIZE);
  std::cout << " full_addr: " << handle_pkt.address;
  std::cout << " full_v_addr: " << handle_pkt.v_address << std::dec;
  std::cout << " type: " << +handle_pkt.type;
  cout<<"set: "<<set<<" way: "<<way<<endl;
}

std::string type_itos(uint64_t typeI){
  switch(typeI){
    case 0:
      return "LOAD";
      break;
    case 1:
      return "RFO";
      break;
    case 2:
      return "PREFETCH";
      break;
    case 3:
      return "WRITEBACK";
      break;
    case 4:
      return "TRANSLATION";
      break;
    default:
      return "UKNOWN TYPE";
      break;
  }
  return "UNKNOWN TYPE";

}
