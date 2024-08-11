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

#include "vmem.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <fstream>
#include <sstream>
#include <numeric>
#include <random>

#include "champsim.h"
#include "util.h"

VirtualMemory::VirtualMemory(uint64_t capacity, uint64_t pg_size, uint32_t page_table_levels, uint64_t random_seed, uint64_t minor_fault_penalty)
    : minor_fault_penalty(minor_fault_penalty), pt_levels(page_table_levels), page_size(pg_size)
      //,ppage_free_list((capacity - VMEM_RESERVE_CAPACITY) / PAGE_SIZE, PAGE_SIZE*N_SOCKETS)
{
  assert(capacity % PAGE_SIZE == 0);
  assert(pg_size == (1ul << lg2(pg_size)) && pg_size > 1024);


  for(int i=0; i<(N_SOCKETS);i++){
    ppage_free_lists[i]=std::deque<uint64_t>((capacity - VMEM_RESERVE_CAPACITY) / PAGE_SIZE, PAGE_SIZE*N_SOCKETS);
    ppage_free_lists[i].front() = VMEM_RESERVE_CAPACITY+pg_size*i;
    //std::cout<<"creating ppage_free_list["<<i<<"]"<<std::endl;
    std::partial_sum(std::cbegin(ppage_free_lists[i]), std::cend(ppage_free_lists[i]), std::begin(ppage_free_lists[i]));
    //sancheck that none of the addresses have CXL bit set
    uint64_t lastpp_addr = ppage_free_lists[i].back();
    uint64_t cxlbit = (lastpp_addr>>CXL_BIT);
    assert(cxlbit==0);
    //std::cout<<"shuffling ppage_free_list["<<i<<"]"<<std::endl;
    std::shuffle(std::begin(ppage_free_lists[i]), std::end(ppage_free_lists[i]), std::mt19937_64{random_seed+i});
  }

  for(int i=0; i<N_SOCKETS;i++){
    next_pte_page[i] = ppage_free_lists[i].front();
    ppage_free_lists[i].pop_front();
  }
  cxl_ppage_free_list=std::deque<uint64_t>((capacity - VMEM_RESERVE_CAPACITY) / PAGE_SIZE, PAGE_SIZE);
  cxl_ppage_free_list.front() = VMEM_RESERVE_CAPACITY+((uint64_t)1<<CXL_BIT);
  //std::cout<<"in init -cxl_ppage_free_list.front() >> CXL_BITS: "<<(cxl_ppage_free_list.front()>>CXL_BIT)<<std::endl;
  std::partial_sum(std::cbegin(cxl_ppage_free_list), std::cend(cxl_ppage_free_list), std::begin(cxl_ppage_free_list));
  std::shuffle(std::begin(cxl_ppage_free_list), std::end(cxl_ppage_free_list), std::mt19937_64{random_seed});

  read_va_to_socket("page_owner_pre.txt");
}

uint64_t VirtualMemory::shamt(uint32_t level) const { return LOG2_PAGE_SIZE + lg2(page_size / PTE_BYTES) * (level); }

uint64_t VirtualMemory::get_offset(uint64_t vaddr, uint32_t level) const { return (vaddr >> shamt(level)) & bitmask(lg2(page_size / PTE_BYTES)); }

uint64_t VirtualMemory::get_dest_socket(uint64_t vaddr, uint32_t socket_num){
  //TODO - use pre-defined table to get dest node?

  uint64_t vpage=vaddr>>LOG2_PAGE_SIZE;
  auto [dest, inserted] = va_to_socket.insert({vpage,(socket_num)});

  if(inserted){///dbg
    //std::cout<<"vpage owner node not found, setting current requester as owner"<<std::endl;
    vp_to_socket_not_found++;
  }
  else{
    vp_to_socket_found++;
  }

  if(dest->second!=CXO){
    if(!(dest->second<N_SOCKETS)){
      std::cout<<"dest node error: "<<dest->second<<std::endl;
    }
    assert(dest->second<N_SOCKETS);
  }
  return dest->second;



  // uint32_t idx = (vaddr >> LOG2_PAGE_SIZE)%(N_SOCKETS*2);
  // if(idx>=N_SOCKETS){ // CXO
  //   return CXO;
  // }
  
  // return idx;

}

std::pair<uint64_t, bool> VirtualMemory::va_to_pa(uint32_t cpu_num, uint64_t vaddr)
{
  uint64_t socket_num = cpu_num/NUM_CPUS;
  uint64_t dest_socket = get_dest_socket(vaddr, (socket_num));
  std::deque<uint64_t> * dqp;
  if(dest_socket==CXO){
    dqp=&cxl_ppage_free_list;
    assert(((dqp->front()>>CXL_BIT)&1)==1);
  }
  else{
    dqp=&(ppage_free_lists[dest_socket]);
    assert(((dqp->front()>>CXL_BIT)&1)==0);
  }

  auto [ppage, fault] = vpage_to_ppage_map.insert({{vaddr >> LOG2_PAGE_SIZE}, dqp->front()});

  // this vpage doesn't yet have a ppage mapping
  if (fault){
    dqp->pop_front();
  }

  return {splice_bits(ppage->second, vaddr, LOG2_PAGE_SIZE), fault};
}

std::pair<uint64_t, bool> VirtualMemory::get_pte_pa(uint32_t cpu_num, uint64_t vaddr, uint32_t level)
{
  uint64_t socket_num = cpu_num/NUM_CPUS;
  //cpu_num=1;
  std::tuple key{socket_num, vaddr >> shamt(level + 1), level};
  auto [ppage, fault] = page_table.insert({key, next_pte_page[socket_num]});

  // this PTE doesn't yet have a mapping
  // if (fault) {
  //   next_pte_page += page_size;
  //   if (next_pte_page % PAGE_SIZE) {
  //     next_pte_page = ppage_free_list.front();
  //     ppage_free_list.pop_front();
  //   }
  // }

  if (fault) { //TODO split next_pte_page for all sockets
  /////////////FIXME/////////

    next_pte_page[socket_num] = ppage_free_lists[socket_num].front();
    ppage_free_lists[socket_num].pop_front();

  }

  return {splice_bits(ppage->second, get_offset(vaddr, level) * PTE_BYTES, lg2(page_size)), fault};
}

int VirtualMemory::remap_page(uint64_t vaddr, uint64_t src_socket, uint64_t dest_socket){

  std::deque<uint64_t> * dqp;
  if(dest_socket==CXO){
    dqp=&cxl_ppage_free_list;
  }
  else{
    dqp=&(ppage_free_lists[dest_socket]);
  }
  
  auto [ppage, inserted] = vpage_to_ppage_map.insert({{vaddr >> LOG2_PAGE_SIZE}, dqp->front()});
  
  if(!inserted){ 
    //std::cout<<"page_remap in (!inserted) conditional"<<std::endl;
    // remove old mapping
    // push back freed ppage
    size_t removed = vpage_to_ppage_map.erase(vaddr >> LOG2_PAGE_SIZE);
    assert(removed==1);
    uint64_t cxlbit = (ppage->second>>CXL_BIT) & 1;
    if(cxlbit==1){
      //assert(src_socket==CXO);
      cxl_ppage_free_list.push_back(ppage->second);
    }
    else{
      uint32_t old_socket = (ppage->second >> LOG2_PAGE_SIZE);
      old_socket=old_socket&(N_SOCKETS-1);
      //assert(old_socket==src_socket);
      assert(old_socket<ppage_free_lists.size());
      ppage_free_lists[old_socket].push_back(ppage->second);
    }

    //retry inserting
    auto [ppage, fault] = vpage_to_ppage_map.insert({{vaddr >> LOG2_PAGE_SIZE}, dqp->front()});
    assert(fault);
  }
  dqp->pop_front();
  remap_count++;
  //std::cout<<"page remapped to "<<dest_channel<<", remap count:"<<remap_count<<std::endl;

  return 0;

}
int VirtualMemory::invalidate_PTES(){
  //TODO fill this out
  return 0;
}
//TODO - invalidate caches as well?

int VirtualMemory::migrate_page(uint64_t vaddr, uint64_t src_socket, uint64_t dest_socket){
  //TODO fill this out
  remap_page(vaddr, src_socket, dest_socket);
  return 0;
}


int VirtualMemory::read_va_to_socket(const string filename){
  std::cout<<"Populating VPage to Socket mapping"<<std::endl;
  std::ifstream file(filename);
  if (!file.is_open()) {
    std::cerr << "Failed to open file: " << filename << std::endl;
    return -1;
  }

  std::string line;
  int count=0;
  while (std::getline(file, line)) {
      std::istringstream iss(line);
      uint64_t key, value;
      if (!(iss >> key >> value)) {
          // Error in parsing, handle appropriately
          break;
      }
      va_to_socket[key] = value;
      count++;
  }
  file.close();
  std::cout<<"mapped "<<count<<" pages"<<std::endl;

  return 0;
}

int VirtualMemory::update_va_to_socket(const string filename){
  prev_va_to_socket=va_to_socket;
  va_to_socket={};
  read_va_to_socket(filename);

  return 0;
}

std::map<uint64_t, std::pair<uint64_t, uint64_t>> VirtualMemory::get_pages_to_migrate(){
  std::map<uint64_t, std::pair<uint64_t, uint64_t>> pages_to_migrate;
  for (const auto &pair : va_to_socket) {
        // Check if the key exists in the prev_va_to_socket map
        if (prev_va_to_socket.count(pair.first)) {
            // Check if the values are different
            if (pair.second != prev_va_to_socket[pair.first]) {
                pages_to_migrate[pair.first] = std::make_pair(prev_va_to_socket[pair.first],pair.second);
            }
        }
    }

    return pages_to_migrate;
}