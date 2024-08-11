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

#include "tb_cpu.h"

#include <algorithm>
#include <vector>

#include "cache.h"
#include "champsim.h"
#include "instruction.h"
#include "vmem.h"
#include <random>

#define DBGPON 0

#define DEADLOCK_CYCLE 1000000\

extern uint8_t warmup_complete[NUM_CPUS];
extern uint8_t MAX_INSTR_DESTINATIONS;
//extern std::array<O3_CPU*, NUM_CPUS> ooo_cpu;
extern uint8_t all_warmup_complete;

extern VirtualMemory vmem;
extern tb_cache_t tb_llcs[N_SOCKETS];
extern coh_directory coh_dir;

bool debug_evict_assertion=false;

void TB_CPU::operate()
{
  //handle_memory_return();          // finalize memory transactions
  //operate_lsq();                   // execute memory transactions
  check_and_action();
  L1D_bus.cur_cycle=current_cycle;
  //std::cout<<"hello?"<<std::endl;

}

int TB_CPU::pack_curpacket(uint64_t buf_val){
  auto [patmp, fault]= vmem.va_to_pa(cpu, buf_val);
  // ///// DBG REMOVE
  // //if (warmup_complete[0]<1) {
  // if(!fault){
  //   if((patmp>>LOG2_PAGE_SIZE & 63)<32){
  //     vmem.remap_page(buf_val, 0, CXO);
  //   }
  // }
  // //just remapped it, so should not fault
  // auto [patmp2, fault2]= vmem.va_to_pa(cpu, buf_val);
  // assert(!fault2);
  // patmp=patmp2;
  //}

  ///////

  curpacket.address=patmp;
  // fill level needed??
  curpacket.fill_level = L1D_bus.lower_level->fill_level;
  curpacket.v_address = buf_val;
  curpacket.to_return={&L1D_bus};
  curpacket.cycle_enqueued=current_cycle;
  //tbcpu id should be offset by number of actually simualted cores
  curpacket.cpu=cpu;
  curpacket.block_transfer=false;
  curpacket.block_owner=N_SOCKETS;


  uint64_t rwbit = buf_val & 1;
	bool isW = rwbit==1;
  if(isW){
    curpacket.type=RFO;
  }
  else{
    curpacket.type=LOAD;
  }

  return 0;

}

int TB_CPU::send_curpacket(){
  //TODO might have to  check if wq and rq accept requests and handle fails
  curpacket.cycle_enqueued=current_cycle;
  bool isW = curpacket.type==RFO;
  //TODO access cache & update coherence directory
  uint64_t socket_id = cpu/NUM_CPUS;
  assert(socket_id!=0);
  uint64_t lineaddr = curpacket.address>>LOG2_BLOCK_SIZE;
  uint64_t is_miss = access_tb_cache(tb_llcs[socket_id],lineaddr, isW, current_cycle, socket_id);
  int is_block_transfer=-1;


  #if ENABLE_COHERENCE
  is_block_transfer = coh_dir.get_coh_owner(curpacket.address, cpu, isW);
  if(is_block_transfer!=-1){
    curpacket.block_transfer=true;
    curpacket.block_owner = is_block_transfer;
  }
  #endif


  //don't acutally send the packet during warmup
  //  ICN_SIM should immediately return the packet anyways, but adding this as precaution
  if(all_warmup_complete < NUM_CPUS){
    return 0;
  }

  //TODO once unfiltered trace is available, 
  //  accesses that hit in cache (and does not require block transfer) don't need to happen
  // if((is_miss==0) && (is_block_transfer==-1)){
  //   return 0;
  // }

  if((is_miss!=0) || (is_block_transfer!=-1)){
    if(isW){
      L1D_bus.lower_level->add_wq(&curpacket);
      store_count++;
    }
    else{
      L1D_bus.lower_level->add_rq(&curpacket);
      load_count++;
      L1D_bus.outstanding_reads++;
    }
    ////DBG
    uint32_t cxl_bit = (curpacket.address>>CXL_BIT) & 1;
    if(cxl_bit==1){
      cxl_access++;
        //cout<<"cpu"<<curpacket.cpu<<" send_curpacket: cxl_access: "<<cxl_access<<std::endl;
    }
    else{
      noncxl_access++;
      //cout<<"cpu"<<curpacket.cpu<<" send_curpacket: noncxl_access: "<<noncxl_access<<std::endl;
    }
  }

  return 0;
}



int TB_CPU::check_and_action(){
  uint64_t cur_i_count = (ooo_cpu[0]->num_retired) + (uint64_t)FORWARD_INSTS;

  //dbg
  // if(cpu==15){
  //   cout<<"TB_CPU(15)::check_and_action: next_issue_i_count: "<<next_issue_i_count<<", cur_i_count: "<<cur_i_count<<endl;
  // }

  while(next_issue_i_count<=cur_i_count){
    //send memory request to ICN, or appropriate lower level

    //TODO - don't send if outstanding accesses == n_oustanding_limit
    if(L1D_bus.outstanding_reads>=LIM_OUTSTANDING_READS){
      consecutive_rob_full_count++;
#if DBGPON
      if((consecutive_rob_full_count%10000==0)&&(consecutive_rob_full_count>0)){cout<<"TBCPU "<<cpu<<" ROB has bee full for "<<consecutive_rob_full_count<<" cycles"<<endl;}
#endif
      break;
    }
    //populate info in to curpacket and send
    send_curpacket();
    //std::cout<<"sendcurpacket 128"<<std::endl;
    consecutive_rob_full_count=0;
    //read trace for addr and timestamp for next request
    char buffer[8];
		uint64_t buf_val=0;
    size_t readsize = read_8B_line(&buf_val, buffer, trace);
    if(buf_val==0xc0ffee){ // phase end flag. discard and read next access
      readsize = read_8B_line(&buf_val, buffer, trace);
      readsize = read_8B_line(&buf_val, buffer, trace);
    }
    if(readsize!=8){cerr<<"readsize returned "<<readsize<<" instead of 8"<<std::endl;}
    uint64_t icount_val=0;
		readsize=read_8B_line(&icount_val, buffer, trace);
    if(readsize!=8){
      cerr<<"readsize returned "<<readsize<<" instead of 8"<<std::endl;
      cerr<<"TBCPU "<<cpu<<" reached end of mtrace, start over"<<endl;
      cout<<"TBCPU "<<cpu<<" reached end of mtrace, start over"<<endl;
      //fseek(trace, 0, SEEK_SET);
      fclose(trace);
      trace = fopen(tfname.str().c_str(), "rb");
      if(FORWARD_INSTS>=(ooo_cpu[0]->num_retired)){
        FORWARD_INSTS = FORWARD_INSTS - (ooo_cpu[0]->num_retired); 
      }
      else{// for phase 0, just stop sending
        next_issue_i_count=~0;
      }
    }
    else{
      pack_curpacket(buf_val);
      next_issue_i_count=icount_val;
    }
    
  }
  return 0;
}

void TBCacheBus::return_data(PACKET* packet)
{
  //std::cout<<"tb_cpu return data"<<std::endl;
  if (all_warmup_complete < NUM_CPUS) {
    if(outstanding_reads>0 && packet->type==LOAD){outstanding_reads--;}
    return;
  }
  if(packet->type==LOAD){
    //assert(outstanding_reads>0);
    if(outstanding_reads<1){
      cout<<"WARNING: TBCPU "<< parent_cpu<<": got return_data but no outstanding reads("<<outstanding_reads<<")"<<endl;
    }
    outstanding_reads--;
  }
  if (packet->type != PREFETCH) {
    //PROCESSED.push_back(*packet);
    // add latency to hist?
    uint64_t lat_tmp =  cur_cycle- packet->cycle_enqueued;
    ///dbg
    //cout<<"cur_cycle: "<<cur_cycle<<", packet->cycle_enqueued: "<<packet->cycle_enqueued<<std::endl;
    //cout<<"lat_tmp: "<<lat_tmp<<std::endl;
    int hist_index = lat_tmp / 24; // divide by 2.4 to convert to ns, then by 10 for histogram(10ns per bucket)
    //std::cout<<"hist index"<<hist_index<<std::endl;
  if(hist_index<0){
      //glitch where cur_cycle is sooner than cycle_enqueued, for some latency ovherhead thing
      // just skip this one
      return;
    }
    if(hist_index>99){ 
      hist_index=99;
    }
    lat_hist[hist_index]++;

    total_lat+=lat_tmp;
    allaccs++;
  }
  
  return;
}
void TBCacheBus::print_lat_hist(){
  // for(int i=0; i<100; i++){
  //   cout<<i*10<<" : "<<lat_hist[i]<<endl;
  // }
  if(allaccs==0){
    cout<<"0 accesses from TB_CPU"<<std::endl;
  }
  else{
    cout<<"avg_lat(ns): "<< (int)((total_lat / allaccs)/2.4)<<std::endl;
    cout<<"(allaccs: "<<allaccs<<")"<<endl;
  }
  /////// cache stats
  //cout<<"TBCPU cache stats: "<<endl;
  //for(int i=0; i<N_SOCKETS;i++){
    int i=9;
    cout<<"TBCPU cache["<<i<<"] hits: "<<tb_llcs[i].hits<<", misses: "<<tb_llcs[i].misses<<", evicts: "<<tb_llcs[i].evicts<<endl;
  //}
  //cout<<endl;
}

void TB_CPU::print_lat_hist(){
  cout<<"\nTB_CPU "<<cpu<<" latency stat"<<std::endl;
  L1D_bus.print_lat_hist();
  cout<<"loads: "<<load_count<<", stores: "<<store_count;
  cout<<",  cxl_accs: "<<cxl_access<<", non_cxl_accs: "<<noncxl_access<<std::endl;
}
void TB_CPU::print_deadlock(){
  cout<<"TB_CPU "<<cpu<<" outstanding reads: "<<L1D_bus.outstanding_reads<<endl;
}

int TB_CPU::forward_to_roi(uint64_t tb_cpu_forawrd, uint64_t warmup_instructions){
  if(cpu==(NUM_CPUS+NUM_TBCPU-1)){
    cout<<"forward_to_roi: after going no-filter for mtrace, this function now just does san-check"<<endl;
  }
  //uint64_t target_i_count = tb_cpu_forawrd+warmup_instructions;
  uint64_t target_i_count = tb_cpu_forawrd+5000;
  uint64_t trace_i_count=0;
  char buffer[8];
  uint64_t buf_val=0;
  uint64_t num_skipped_accesses=0;
  FORWARD_INSTS = tb_cpu_forawrd;
  while(trace_i_count<target_i_count){
    num_skipped_accesses++;
    size_t readsize = read_8B_line(&buf_val, buffer, trace);
    if(buf_val==0xc0ffee){ // phase end flag. discard and read next access
      readsize = read_8B_line(&buf_val, buffer, trace);
      readsize = read_8B_line(&buf_val, buffer, trace);
    }
    if(readsize!=8){cerr<<"readsize returned "<<readsize<<" instead of 8"<<std::endl;}
    readsize=read_8B_line(&trace_i_count, buffer, trace);
  }
  next_issue_i_count=trace_i_count;
  pack_curpacket(buf_val);
  if(cpu==(NUM_CPUS+NUM_TBCPU-1)){
    cout<<"skipped traces from the first "<<target_i_count<<" instructions, skipped "<<num_skipped_accesses<<" accesses" <<std::endl;
    cout<<"next_issue_i_count= "<<next_issue_i_count<<std::endl;
  }

  return 0;
}



uint64_t access_tb_cache(tb_cache_t& cach, uint64_t lineaddr, bool isW, uint64_t ts, uint32_t socketid){
  //return 0 for hit, 1 for miss
    //uint64_t lineaddr = addr>>LINEBITS;
	  uint64_t set_index = lineaddr & (TB_LLC_SETS-1); //only works if NUM_SETS is power of 2
  
    //find hit
    uint64_t hit_i=0;
    bool ishit = false;
    //find inval
    uint64_t insert_i=0;
    bool i_entry_found = false;
    //find lru
    uint64_t lru_ts = ~0;
    uint64_t lru_i = 0;
    //assert(999999999999<lru_ts); //dbg

    //TODO check for hit first!
    for(int j=0; j<TB_LLC_WAYS;j++){
        if(cach.entries[set_index][j].valid==false){
            insert_i=j;
            i_entry_found=true;
            //break;
        }
        else if(cach.entries[set_index][j].tag==lineaddr){
            hit_i=j;
            ishit=true;
            break;            
        }
        //finding lru
        if(cach.entries[set_index][j].ts<lru_ts){
            lru_ts = cach.entries[set_index][j].ts;
            lru_i=j;
        }
    }

	  //update coherence directory here?
	

    if(ishit){
        if(isW){
            cach.entries[set_index][hit_i].dirty=true;
			      cach.entries[set_index][hit_i].cstate=M;			            
        }
        cach.entries[set_index][hit_i].ts=ts;
        cach.hits++;
		    //TODO update_directory(socketid, lineaddr, set_index, isW);
        return 0;
    }

    cach.misses++;
    if(i_entry_found==false){ //need to evict
        cach.evicts++;
        insert_i = lru_i;
        // handle evicted line directory/coherence:
        // if state is E or M --> update directory state to I
        // if state is M --> m_write++;

      uint64_t evicted_lineaddr = cach.entries[set_index][lru_i].tag;
      uint64_t evicted_addr_ext = evicted_lineaddr << LOG2_BLOCK_SIZE;
#if ENABLE_COHERENCE
      if(!debug_evict_assertion){
      coh_dir.update_evict(evicted_addr_ext, socketid*NUM_CPUS, "TBCPU_LLC");
      }
#endif
      // rest is dealing with coherence from eviction
      //  can probably be replaced with coh_dir.update_evict()

      // DirectoryEntry &ede = CCD[set_index][evicted_lineaddr];


      // if(cach.entries[set_index][lru_i].dirty==true){
      //   //incrmenet mem_wr, set directory state to I, unset all sharers
      //   //dbg
      //   assert(cach.entries[set_index][lru_i].cstate==M);
      //   assert(ede.state==M);
      //   assert(ede.owner==socketid);

      //   ede.sharers[socketid]=false;
      //   ede.state=I;
      // }
      // else{
      //   //if E, and owner is self, set invalid
      //   if(ede.state==E){
      //     //dbg
      //     assert(ede.owner==socketid);
      //     assert(cach.entries[set_index][lru_i].cstate==E);
      //     ede.sharers[socketid]=false;
      //     ede.state=I;
      //   }
      //   else{
      //     assert(ede.state==S);
      //     ede.sharers[socketid]=false;
      //     uint64_t numsharers=0;
      //     for(uint32_t jj=0; jj<N_SOCKETS;jj++){
      //       if(ede.sharers[jj]){
      //         numsharers++;
      //         if(jj!=socketid){ede.owner=jj;}
      //       }
      //     }
      //     assert(ede.owner!=socketid); // should have at least one other node if in S
      //     if(numsharers==1){
      //       ede.state=E;
      //       for(int ii=0; ii<TB_LLC_WAYS;ii++){
      //         if((caches[ede.owner].entries[set_index][ii].tag==evicted_lineaddr) && (caches[ede.owner].entries[set_index][ii].valid)){
      //           caches[ede.owner].entries[set_index][ii].cstate=E;
      //           break;
      //         }
      //       }					
      //     }
      //   }
      // }

    }


    cach.entries[set_index][insert_i].tag=lineaddr;
    cach.entries[set_index][insert_i].valid=true;
    cach.entries[set_index][insert_i].ts = ts;
    cach.entries[set_index][insert_i].dirty=false;
	  cach.entries[set_index][insert_i].cstate=coh_state_t::E;
    if(isW){ 
		  cach.entries[set_index][insert_i].dirty=true;
		  cach.entries[set_index][insert_i].cstate=coh_state_t::M;
	  }
    //TODO handle directory/coherence
    
	//std::cout<<"inserted index: "<<insert_i<<endl;
	//TODO update_directory(socketid, lineaddr, set_index, isW);
  
  // more coherence checking - again, probably can just be replaced with update_directory,
  //  but double check for extra safety

	// if(CCD[set_index][lineaddr].state==S){
	// 	assert(!isW);
	// 	cach.entries[set_index][insert_i].cstate=S;
	// }

	// assert(CCD[set_index][lineaddr].state == cach.entries[set_index][insert_i].cstate);

	// //dbg
	// if(cach.entries[set_index][insert_i].cstate==S) assert(cach.entries[set_index][insert_i].dirty==false);

    return 1; //return 1 for miss
}



uint64_t inval_tb_cache(tb_cache_t& cach, uint64_t lineaddr, uint32_t socketid){
  uint64_t set_index = lineaddr & (TB_LLC_SETS-1); //only works if NUM_SETS is power of 2

  for(int j=0; j<TB_LLC_WAYS;j++){
        if(cach.entries[set_index][j].tag==lineaddr){
          if(cach.entries[set_index][j].valid==false){
            cout<<"invalidating but the line is not there"<<endl;
            cout<<"lineaddr: "<<lineaddr<<", socketId: "<<socketid<<endl;
          }
          assert(cach.entries[set_index][j].valid);
          cach.entries[set_index][j].valid=false;
          cach.entries[set_index][j].dirty=false;
          cach.entries[set_index][j].tag=0;
          break;
        }
  }
  return 0;
}
