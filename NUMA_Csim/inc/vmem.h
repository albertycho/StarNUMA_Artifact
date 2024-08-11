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

#ifndef VMEM_H
#define VMEM_H

#include <cstdint>
#include <deque>
#include <map>
#include <unordered_map>
#include "champsim.h"

// reserve 1MB of space
#define VMEM_RESERVE_CAPACITY 1048576

#define PTE_BYTES 8

//TODO move these to champsim_constants.h, make them configurable?
#define CH_PER_SOCKET 2

#define CH_ON_CXL_ISLAND 4

class VirtualMemory
{
private:
  //std::map<std::pair<uint32_t, uint64_t>, uint64_t> vpage_to_ppage_map;
  std::map<uint64_t, uint64_t> vpage_to_ppage_map={};
  std::map<std::tuple<uint32_t, uint64_t, uint32_t>, uint64_t> page_table={};

  uint64_t next_pte_page[N_SOCKETS];

public:
  const uint64_t minor_fault_penalty;
  const uint32_t pt_levels;
  const uint32_t page_size; // Size of a PTE page
  //std::deque<uint64_t> ppage_free_list;
  //std::array<std::deque<uint64_t>, ((N_SOCKETS+CH_ON_CXL_ISLAND)*CH_PER_SOCKET)> ppage_free_lists;
  std::array<std::deque<uint64_t>, ((N_SOCKETS))> ppage_free_lists;
  std::deque<uint64_t> cxl_ppage_free_list;

  uint64_t remap_count=0;
  uint64_t vp_to_socket_found=0, vp_to_socket_not_found=0;

  //dunno why but using unordered map breaks things..
  std::map<uint64_t, uint64_t> va_to_socket;
  std::map<uint64_t, uint64_t> prev_va_to_socket;


  // capacity and pg_size are measured in bytes, and capacity must be a multiple
  // of pg_size
  VirtualMemory(uint64_t capacity, uint64_t pg_size, uint32_t page_table_levels, uint64_t random_seed, uint64_t minor_fault_penalty);
  uint64_t shamt(uint32_t level) const;
  uint64_t get_offset(uint64_t vaddr, uint32_t level) const;
  std::pair<uint64_t, bool> va_to_pa(uint32_t cpu_num, uint64_t vaddr);
  std::pair<uint64_t, bool> get_pte_pa(uint32_t cpu_num, uint64_t vaddr, uint32_t level);
  int remap_page(uint64_t vaddr, uint64_t src_socket, uint64_t dest_socket);
  int invalidate_PTES();
  int migrate_page(uint64_t vaddr, uint64_t src_socket, uint64_t dest_socket);
  uint64_t get_dest_socket(uint64_t vaddr, uint32_t cpu_num);
  int read_va_to_socket(const string filename);
  int update_va_to_socket(const string filename);
  std::map<uint64_t, std::pair<uint64_t, uint64_t>> get_pages_to_migrate();
};

#endif
