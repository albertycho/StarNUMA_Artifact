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

#include "migrator.h"

#include <algorithm>
#include <vector>

#include "cache.h"
#include "champsim.h"
#include "instruction.h"
#include "vmem.h"
#include "icn_sim.h"
#include <random>

#include "coh_directory.h"

#define DEADLOCK_CYCLE 1000000


extern uint8_t warmup_complete[NUM_CPUS];
extern uint8_t MAX_INSTR_DESTINATIONS;
extern uint64_t TLB_shootdown_stall[NUM_CPUS]; 
//extern uint64_t wait_till_next_TLB_shootdown[NUM_CPUS];
extern uint64_t wait_till_next_TLB_shootdown;
//extern std::array<O3_CPU*, NUM_CPUS> ooo_cpu;
extern uint8_t all_warmup_complete;
extern bool migration_all_done;
extern std::array<CACHE*, NUM_CACHES> caches;

extern VirtualMemory vmem;
extern ICN_SIM ICN;
extern coh_directory coh_dir;

void MIGRATOR::operate()
{

  //handle_memory_return();          // finalize memory transactions
  //operate_lsq();                   // execute memory transactions
  check_and_action();
  L1D_bus.cur_cycle=current_cycle;
  //std::cout<<"hello?"<<std::endl;
  #if MODEL_MIGRATION_OVERHEAD
  send_migration_lines();
  #endif
  if(wait_till_next_TLB_shootdown>0){
    wait_till_next_TLB_shootdown--;
  }
  // for(uint64_t ii=0; ii<NUM_CPUS;ii++){
  //   if(wait_till_next_TLB_shootdown[ii]>0){
  //     wait_till_next_TLB_shootdown[ii]--;
  //   }
  // }


}

int MIGRATOR::check_and_action(){
  //// calls start_migration once early on in the simulation
  if(migration_called){
    return -1;
  }
  if (all_warmup_complete < NUM_CPUS){
    return -1;
  }
    ///DBG  - REMOVE!!!!!!!!!!!
  // uint64_t cur_i_count = (ooo_cpu[0]->num_retired);
  // if(cur_i_count < 8000000){
  //   return -1;
  // }

  start_migration();
  migration_called=true;

  return 0;
}

int MIGRATOR::start_migration(){

  //read next VPAGE to Socket owners
  std::string next_phase_vpage_to_socket_filename = "page_owner_post.txt";
  vmem.update_va_to_socket(next_phase_vpage_to_socket_filename);
  // populate a list of pages that need to migrate
  pages_to_migrate=vmem.get_pages_to_migrate();
  cout<<"migration start, migrating "<<pages_to_migrate.size()<<" pages, curcycle "<<current_cycle<<std::endl;
  

  //to be precise, would have to call remap after migration is done. 
  //  This gets too complicated, so for now being content with modeling the overhead
  //  and migrate everything here
  for (const auto& pair : pages_to_migrate) {
    uint64_t vaddr = pair.first << LOG2_PAGE_SIZE;
    uint64_t src_socket = pair.second.first;
    uint64_t dest_socket = pair.second.second;
    //uint64_t old_paddr = vmem.va_to_pa(0,vaddr).first;
    vmem.remap_page(vaddr, src_socket, dest_socket);
    //shootdown_TLB(vaddr, old_paddr);
  }
  ///invalidate TLBs in simulated cores

  // maybe populate a list of transactions needed for each page migration
  //#if MODEL_MIGRATION_OVERHEAD
  populate_migration_lines();
  //#endif

  std::cout<<"start_migration done"<<std::endl;
  return 0;
}
int MIGRATOR::populate_migration_lines(){
  uint64_t num_migration_lines=0;
      for (const auto& pair : pages_to_migrate) {
        uint64_t page_start = pair.first <<LOG2_PAGE_SIZE;  // Compute physical address of the start of the page
        uint64_t page_end = page_start + PAGE_SIZE;  // Compute physical address of the end of the page
        
        uint64_t dest = pair.second.second;
        if(dest==CXO) dest=N_SOCKETS; //last index of array of vectors allocated for CXL island
        assert(dest<N_SOCKETS+1);
        // Now compute the physical addresses of each cacheline in the page and add them to the vector
        for (uint64_t line_start = page_start; line_start < page_end; line_start += 64) {
            migration_lines[dest].push_back(line_start);
            num_migration_lines++;
            //FIXME - TLB SHOOTDOWN to move to send_migration lines TODO
            #if !MODEL_MIGRATION_OVERHEAD                                       
              shootdown_TLB(line_start, line_start);                              
            #endif    
            
        }
    }
    std::cout<<"num migration lines "<<num_migration_lines<<std::endl;

    return 0;
}
int MIGRATOR::send_migration_lines(){
  for(uint32_t ii=0;ii<NUM_CPUS;ii++){ // if any core still processing TLB shootdown, pause migration
    if(TLB_shootdown_stall[ii]!=0){
      return 0;
    }
  }

  uint64_t migrated_lines=0;
  uint64_t remaining_lines=0;
  //uint64_t arrsize = migration_lines.size();
  uint64_t arrsize = N_SOCKETS+1;
  for(uint64_t i=0;i<arrsize;i++){
    remaining_lines=remaining_lines+ migration_lines[i].size();
    while(!migration_lines[i].empty()){

      uint64_t v_address=migration_lines[i].front();
      uint64_t vpage = v_address>>LOG2_PAGE_SIZE;
      uint64_t src=pages_to_migrate[vpage].first;
      uint64_t dest=pages_to_migrate[vpage].second;
      
      //if(i!=16) assert(dest==i);

      if(mig_in_flight[i]!=0){
        if(mig_in_flight[i]!=vpage){ //another page in-flight. wait (for this dest)
          break;
        }

      }

      if(!ICN.can_take(src,dest,false)){
        //cout<<"sendlines - ICN can't take for src: "<<src<<", dest: "<<dest<<endl;
        break;
      }

      PACKET next_packet;
      next_packet.v_address=v_address;
      next_packet.cpu=pages_to_migrate[vpage].first;//src

      ///TODO call shootdown_TLB here
      for(uint32_t ii=0;ii<NUM_CPUS;ii++){
        //reset TLBshotdown info
        TLB_shotdown[ii]=false;
      }
      probe_TLBs(next_packet.v_address);
      bool TLB_shootdown_cooldown=false;
      for(uint32_t ii=0;ii<NUM_CPUS;ii++){
        if(TLB_shotdown[ii]){
          if(wait_till_next_TLB_shootdown>0){
            if(mig_in_flight[i]==0){
              TLB_shootdown_cooldown=true;
            }
          }          
        }
      }

      if(TLB_shootdown_cooldown){
        break;
      }


      shootdown_TLB(next_packet.v_address, next_packet.v_address);
      auto [patmp, fault]= vmem.va_to_pa(next_packet.cpu, next_packet.v_address);
      //dbg 
      //if(fault) std::cout<<"VA to PA assigned during send_migration"<<endl;
      next_packet.address = patmp;

      next_packet.type=WRITEBACK;
      //next_packet.type=LOAD;
      next_packet.instr_id=MIGRATION_INSID;//for DBG...
      next_packet.cycle_enqueued=current_cycle;

  	  next_packet.to_return={&L1D_bus};

      //std::cout<<"in send migration_lines line 178 before addrq, i: "<<i<<std::endl;
      //std::cout<<"src cpu: "<<next_packet.cpu<<std::endl;
      //WA for now: ignore migration from CXL to socket..
      if(next_packet.cpu!=100){
        if(L1D_bus.lower_level->add_wq(&next_packet)==-2){
          // break if link is full
          //cout<<"sendLines - addrq to ICN failed: "<<src<<", dest: "<<dest<<endl;
          break;
        }  
        if(mig_in_flight[i]==0){ // set in-flight to this page
          mig_in_flight[i]=vpage;
          in_flight_remaining_pages[i]=BLOCKS_PER_PAGE; //number of blocks in a page
          in_flights_start_time[i]=current_cycle;
          //TODO model TLB shootdown penalty for this case
          #if MODEL_TLB_SD_OVERHEAD
          uint32_t initiator = rand() % (NUM_CPUS+NUM_TBCPU);
          for(uint32_t ii=0;ii<NUM_CPUS;ii++){
            if(TLB_shotdown[ii]){
              if(TLB_shootdown_stall[ii]!=0){
                std::cerr<<"TLB shootdown stall["<<ii<<"] remains: "<<TLB_shootdown_stall[ii]<<endl;
              }
              TLB_shootdown_stall[ii]=3000;        
              wait_till_next_TLB_shootdown=6000;
              if(initiator==ii){
                TLB_shootdown_stall[ii]=13000;
                wait_till_next_TLB_shootdown=16000;
              }   
            }
          }
          #endif
        }
      }
      migrated_lines++;
      migration_lines[i].erase(migration_lines[i].begin());
    }
  }
  // not using this now, but could be useful in dbug
  // std::cout<<"remaining_lines "<<remaining_lines<<std::endl;
  // std::cout<<"migrated lines "<<migrated_lines<<std::endl;
  //// DBG
  if(remaining_lines!=0){
    uint64_t remaining_lines_post=0;
    for(uint64_t i=0;i<arrsize;i++){
      remaining_lines_post=remaining_lines_post+ migration_lines[i].size();
    }
    if(remaining_lines_post==0){
      std::cout<<"send_lines: All lines sent"<<std::endl;
      std::cout<<"migrated_lines: "<<migrated_lines<<std::endl;
      std::cout<<"returned lines: "<<L1D_bus.allaccs<<std::endl;
      //ICN.print_deadlock();
    }
  }

  return migrated_lines;

}

int MIGRATOR::probe_TLBs(uint64_t vaddr){
    for (CACHE* cache : caches) {
      // If "TLB" is in the cache's NAME
      if (cache->NAME.find("TLB") != std::string::npos) {
          // Call invalidate_entry
          bool probe_hit = cache->probe_entry(vaddr);
          if(probe_hit){
             size_t cpuPos = cache->NAME.find("cpu");
            if(cpuPos!=std::string::npos){
              size_t cpuIdPos = cpuPos + 3; // Move the position to right after "cpu" (which is 3 characters long)
              size_t endPos = cache->NAME.find("_", cpuIdPos); // Look for the next "_" character after "cpu"
              std::string cpuIdStr = cache->NAME.substr(cpuIdPos, endPos - cpuIdPos);
              uint64_t cpuId = std::stoi(cpuIdStr);
              if(cpuId>NUM_CPUS){
                std::cout<<"cpuID went over NUM_CPUS"<<std::endl;
              }
              else{
                TLB_shotdown[cpuId]=true;
              }
            }
          }
      }
    }

  return 0;
}

int MIGRATOR::shootdown_TLB(uint64_t vaddr, uint64_t old_paddr){
  //invalidate entry with vaddr for TLBs
  // old p_addr for other caches
  // TLB keeps line by line in Champsim, (right impl is probably per page..)

  for (CACHE* cache : caches) {
      // If "TLB" is in the cache's NAME
      if (cache->NAME.find("TLB") != std::string::npos) {
          // Call invalidate_entry
          int inval_hit = cache->invalidate_entry(vaddr);
          if(inval_hit!=-1){
            size_t cpuPos = cache->NAME.find("cpu");
            if(cpuPos!=std::string::npos){
              size_t cpuIdPos = cpuPos + 3; // Move the position to right after "cpu" (which is 3 characters long)
              size_t endPos = cache->NAME.find("_", cpuIdPos); // Look for the next "_" character after "cpu"
              std::string cpuIdStr = cache->NAME.substr(cpuIdPos, endPos - cpuIdPos);
              uint64_t cpuId = std::stoi(cpuIdStr);
              if(cpuId>NUM_CPUS){
                std::cout<<"cpuID went over NUM_CPUS"<<std::endl;
              }
              else{
                TLB_shotdown[cpuId]=true;
              }
            }
          }
          
          //dbg
          //std::cout<<"TLB found in name: "<<cache->NAME<<std::endl;
      }
      // else{ // Ignoring regular caches
      //   //call invalidate on old physical address
      //   cache->invalidate_entry(old_paddr);
      //   //std::cout<<"Regular cache: "<<cache->NAME<<std::endl;
      // }
  }
  return 0;
}



void MIGRATORBus::return_data(PACKET* packet)
{
  //std::cout<<"tb_cpu return data"<<std::endl;
  if (all_warmup_complete < NUM_CPUS) {
    return;
  }

  uint64_t vpage = (packet->v_address) >>LOG2_PAGE_SIZE;
  bool not_in_flight_dbg=true;
  for(uint64_t i =0; i<N_SOCKETS+1;i++){
    if(parent->mig_in_flight[i]==vpage){
      not_in_flight_dbg=false;
      parent->in_flight_remaining_pages[i]=parent->in_flight_remaining_pages[i]-1;
      if(parent->in_flight_remaining_pages[i]==0){
        parent->mig_in_flight[i]=0;
        //TODO stat tracking for migration time
        uint64_t page_migration_time = cur_cycle-(parent->in_flights_start_time[i]);
        parent->n_migrated_pages++;
        parent->migration_time_sum=(parent->migration_time_sum)+page_migration_time;
        if(i==N_SOCKETS){ //target is CXL Island
          parent->n_migrated_pages_CXL++;
          parent->migration_time_sum_CXL=(parent->migration_time_sum_CXL)+page_migration_time;
        }
      }
    }
  }
  if(not_in_flight_dbg){
    cout<<"WARNING - MIGRATORBus::return_data: didn't find a matching in flight page"<<std::endl;
  }
    //if (packet->type != PREFETCH) {
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
    //}
  
  return;
}
void MIGRATORBus::print_lat_hist(){
  // for(int i=0; i<100; i++){
  //   cout<<i*10<<" : "<<lat_hist[i]<<endl;
  // }
  if(allaccs==0){
    std::cout<<"0 accesses from TB_CPU"<<std::endl;
  }
  else{
    std::cout<<"avg_lat per line(ns): "<< (int)((total_lat / allaccs)/2.4)<<", num lines sent: "<<allaccs<<std::endl;
  }
}

void MIGRATOR::print_lat_hist(){
  cout<<"\nMigrator "<<cpu<<" latency stat"<<std::endl;
  L1D_bus.print_lat_hist();
  uint64_t remaining_lines = 0;
  for(uint64_t i=0; i<migration_lines.size();i++){
    remaining_lines+=migration_lines[i].size();
  }
  if(n_migrated_pages>0){
    std::cout<<"AVG Migration Time(ns): "<<(int)((migration_time_sum / n_migrated_pages)/2.4)<<endl;
  }
  if(n_migrated_pages_CXL>0){
    std::cout<<"AVG Migration Tme to CXL(ns): "<<(int)((migration_time_sum_CXL / n_migrated_pages_CXL)/2.4)<<endl;
  }
  cout<<"Remaining Lines that didn't get migrated: "<<remaining_lines<<std::endl;
}



///DBG
uint64_t MIGRATOR::get_remaining_lines(){
 uint64_t remaining_lines=0;
    uint64_t arrsize = N_SOCKETS+1;
    for(uint64_t i=0;i<arrsize;i++){
      remaining_lines=remaining_lines+ migration_lines[i].size();
    }
    return remaining_lines;  
}
