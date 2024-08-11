/*
 * Copyright (C) 2004-2021 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

/*! @file
 *  This file contains an ISA-portable PIN tool for functional simulation of
 *  instruction+data TLB+cache hierarchies
 */

#include <iostream>
#include <string>
#include <sstream>
#include "pin.H"
#include <fstream>
#include <stdlib.h>
#include <string.h>

typedef UINT32 CACHE_STATS; // type of cache hit/miss counters

#include "pin_cache_ac.H"
#include "trace_instruction.h"
using trace_instr_format_t = input_instr;
#define MAX_THREADS 64


trace_instr_format_t curr_instr;
THREADID champsim_trace_tid = 15; // thr 16 should be a thread of interest for us, for most apps

std::ofstream champsim_outfile;

//FILE * trace_sancheck;
FILE * trace[MAX_THREADS];
//FILE * ins_trace[MAX_THREADS];
//KNOB<std::string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "memtrace.out", "specify output file name");
KNOB< BOOL > KnobStartFF(KNOB_MODE_WRITEONCE, "pintool", "startFF", "0", "no trace until ROI start indicated");
KNOB<UINT64> KnobSkipInstructions(KNOB_MODE_WRITEONCE, "pintool", "s", "0", 
        "How many instructions to skip before tracing begins");

KNOB<UINT64> KnobTraceInstructions(KNOB_MODE_WRITEONCE, "pintool", "t", "1000000", 
        "How many instructions to trace");

BOOL     inROI[MAX_THREADS];
BOOL	 inROI_master;
uint64_t ins_count[MAX_THREADS] = {};
uint64_t memref_single_count=0;
uint64_t memref_multi_count=0;
uint64_t num_maccess[MAX_THREADS] = {};

uint64_t champsim_skipins=0;
uint64_t champsim_traceins=2000000000; //2B inst by default
uint64_t champsim_tracedoneins=2000000000; //skipins+traceins
bool champsim_trace_done=false;

//intermediate data structure to batch write to file
#define TBUF_SIZE 1024
//uint64_t t_buf[TBUF_SIZE];
//uint64_t tb_i = 0;
uint64_t t_buf[MAX_THREADS][TBUF_SIZE];
uint8_t rw_buf[MAX_THREADS][TBUF_SIZE];
uint64_t timestamp_buf[MAX_THREADS][TBUF_SIZE];
uint64_t tb_i[MAX_THREADS] = {};

uint64_t numThreads = 0;


namespace IL1
{
// 1st level instruction cache: 32 kB, 32 B lines, 32-way associative
const UINT32 cacheSize                         = 32 * KILO;
const UINT32 lineSize                          = 32;
const UINT32 associativity                     = 32;
const CACHE_ALLOC::STORE_ALLOCATION allocation = CACHE_ALLOC::STORE_NO_ALLOCATE;

const UINT32 max_sets          = cacheSize / (lineSize * associativity);
const UINT32 max_associativity = associativity;

typedef CACHE_ROUND_ROBIN(max_sets, max_associativity, allocation) CACHE;
} // namespace IL1
//static IL1::CACHE il1("L1 Instruction Cache", IL1::cacheSize, IL1::lineSize, IL1::associativity);
//static IL1::CACHE *il1=new IL1::CACHE("L1 Instruction Cache", IL1::cacheSize, IL1::lineSize, IL1::associativity);
static IL1::CACHE *il1[MAX_THREADS];

namespace UL2
{
// 2nd level unified cache: 2 MB, 64 B lines, direct mapped
const UINT32 cacheSize                         = 2 * MEGA;
const UINT32 lineSize                          = 64;
const UINT32 associativity                     = 8;
const CACHE_ALLOC::STORE_ALLOCATION allocation = CACHE_ALLOC::STORE_ALLOCATE;

const UINT32 max_sets = cacheSize / (lineSize * associativity);

//typedef CACHE_DIRECT_MAPPED(max_sets, allocation) CACHE;
typedef CACHE_ROUND_ROBIN(max_sets, associativity, allocation) CACHE;
} // namespace UL2
static UL2::CACHE *ul2[MAX_THREADS];

namespace UL3
{
// 3rd level unified cache: 16 MB, 64 B lines, direct mapped
//const UINT32 cacheSize                         = 32 * MEGA;
const UINT32 cacheSize                         = 4 * MEGA; //per socket
const UINT32 lineSize                          = 64;
const UINT32 associativity                     = 8;
const CACHE_ALLOC::STORE_ALLOCATION allocation = CACHE_ALLOC::STORE_ALLOCATE;

const UINT32 max_sets = cacheSize / (lineSize * associativity);

//typedef CACHE_DIRECT_MAPPED(max_sets, allocation) CACHE;
typedef CACHE_ROUND_ROBIN(max_sets, associativity, allocation) CACHE;
} // namespace UL3
//static UL3::CACHE ul3("L3 Unified Cache", UL3::cacheSize, UL3::lineSize, UL3::associativity);
static UL3::CACHE *ul3[MAX_THREADS];



static inline VOID dump_tbuf(THREADID tid) { //TODO add threaID to arg
    //added this as an optimization, but doesn't seem to save runtime much :(
    uint64_t tmp_tbi = tb_i[tid];
    for (uint64_t i = 0; i < tmp_tbi; i++) {
        //original ver where trace was in text
        //fprintf(trace[tid], "%p %d\n", (void*)(t_buf[tid][i]), rw_buf[tid][i]);
        //fprintf(trace_sancheck, "%p %d\n", (void*)(t_buf[tid][i]), rw_buf[tid][i]);

        //we don't care about last few bits of addr, so pack RW info in there
        //uint64_t lsb_unsetter = ~0xF;
        uint64_t addr_rw = t_buf[tid][i] & ~0xF;
        addr_rw = addr_rw+rw_buf[tid][i];
        uint64_t timestamp = timestamp_buf[tid][i];
        fwrite(&addr_rw, sizeof(uint64_t), 1,trace[tid]);
        fwrite(&timestamp, sizeof(uint64_t), 1,trace[tid]);
    }
    tb_i[tid] = 0;
}

static VOID Fini(int code, VOID* v) //TODO this should change to threadfini?
{
    //dump_tbuf();
    //std::cout << "num mem accesses: "<<num_maccess <<std::endl;
    std::cout << "Fini finished"<<std::endl;

    FILE * doneIndicator = fopen("this_run_is_done.txt", "w");
	fprintf(doneIndicator, "This run is done!");
	fclose(doneIndicator);

}
VOID ThreadFini(THREADID tid, const CONTEXT* ctxt, INT32 code, VOID* v) {
    dump_tbuf(tid);
    std::cout <<"thread_"<<tid << " num mem accesses: " << num_maccess[tid] << std::endl;
    std::cout << "thread_" << tid << " Fini finished" << std::endl;
    fclose(trace[tid]);
    //fclose(ins_trace[tid]);
}

static inline VOID recordAccess(ADDRINT addr, THREADID tid, CACHE_BASE::ACCESS_TYPE accessType) { //TODO add threaID to arg
    num_maccess[tid]++;
    t_buf[tid][tb_i[tid]] = addr;
	rw_buf[tid][tb_i[tid]] = accessType;
    timestamp_buf[tid][tb_i[tid]] = ins_count[tid];
    tb_i[tid]++;
    if (tb_i[tid] == TBUF_SIZE) {
        dump_tbuf(tid);
    }
}

static inline VOID Ul3Access(ADDRINT addr, UINT32 size, CACHE_BASE::ACCESS_TYPE accessType, THREADID tid, bool dirtyEv_fromL2, bool isIns=false) ////TODO add threaID to arg
{
    ADDRINT evicted_line=0;
    //const BOOL ul3hit = ul3[tid]->Access(addr, size, accessType);
    const BOOL ul3hit = ul3[tid]->Access(addr, size, accessType, evicted_line);
    if(!ul3hit){
        if(!dirtyEv_fromL2){ // if access is from dirtyEvict, it just installs the line from L2, no memory access
            recordAccess(addr, tid, CACHE_BASE::ACCESS_TYPE_LOAD); //access is read whether load or store. Write is only when dirty eviction
        }
        //if(isIns){ //skipping ins_trace for now
        //    fprintf(ins_trace[tid], "%p\n", (void*)(addr));
        //}
        if(evicted_line!=0){
            recordAccess(evicted_line, tid, CACHE_BASE::ACCESS_TYPE_STORE);
        }
    }
}

static VOID Ul2Access(ADDRINT addr, UINT32 size, CACHE_BASE::ACCESS_TYPE accessType, THREADID tid, bool isIns=false)
{
    ADDRINT evicted_line=0;
    // second level unified cache
    const BOOL ul2Hit = ul2[tid]->Access(addr, size, accessType, evicted_line);

    // third level unified cache
    if(!ul2Hit) {
        Ul3Access(addr, size, accessType, tid, false, isIns);
        //writeback (to next level, L3)
        if(evicted_line!=0){
            Ul3Access(evicted_line, 1, CACHE_BASE::ACCESS_TYPE_STORE, tid, true, false);
        }
    }
}

BOOL ShouldWrite(THREADID tid)
{//TODO rewrite

  if(tid!=champsim_trace_tid){
        return false;
  }
  //std::cout<<"shouldWrite gets here1"<<std::endl;
  if(!(inROI[champsim_trace_tid] || inROI_master)){
    return false;
  }
  if(ins_count[champsim_trace_tid] > champsim_tracedoneins){
  //std::cout<<"shouldWrite: inscount: "<<ins_count[champsim_trace_tid]<<std::endl;
    if(!champsim_trace_done){
	  champsim_outfile.close();
      champsim_trace_done=true;
    }
    return false;
	  //exit(0);
  }
  //std::cout<<"shouldWrite gets here2"<<std::endl;
  return (ins_count[champsim_trace_tid] > champsim_skipins);
  //return (instrCount > KnobSkipInstructions.Value()) && (instrCount <= (KnobTraceInstructions.Value()+KnobSkipInstructions.Value()));
}

void WriteCurrentInstruction(THREADID tid)
{
    if(tid!=champsim_trace_tid){
        return;
    }
//   if(!inROI[champsim_trace_tid]){
//     return;
//   }
//   if(ins_count[champsim_trace_tid] > champsim_tracedoneins){
//     if(!champsim_trace_done){
// 	  champsim_outfile.close();
//       champsim_trace_done=true;
//     }
//     return;
// 	  //exit(0);
//   }
//   if (ins_count[champsim_trace_tid] < champsim_skipins){
//     return;
//   }
  typename decltype(champsim_outfile)::char_type buf[sizeof(trace_instr_format_t)];
  std::memcpy(buf, &curr_instr, sizeof(trace_instr_format_t));
  champsim_outfile.write(buf, sizeof(trace_instr_format_t));
}
void BranchOrNot(UINT32 taken, THREADID tid)
{
    if(tid!=champsim_trace_tid){
        return;
    }
    curr_instr.is_branch = 1;
    curr_instr.branch_taken = taken;
}
template <typename T>
void WriteToSet(T* begin, T* end, UINT32 r, THREADID tid)
{
    if(tid!=champsim_trace_tid){
        return;
    }
    if(champsim_trace_done){ return;}
    if(ins_count[champsim_trace_tid] < champsim_skipins){
        return;
    }
  auto set_end = std::find(begin, end, 0);
  auto found_reg = std::find(begin, set_end, r); // check to see if this register is already in the list
  *found_reg = r;
}
int dummy1=0;
template <typename T>
void WriteToSet_mem(T* begin, T* end, UINT64 r, THREADID tid)
{
    if(tid!=champsim_trace_tid){
        return;
    }
    if(champsim_trace_done){ return;}
    if(ins_count[champsim_trace_tid] < champsim_skipins){
        return;
    }
    ///dbg
    if(dummy1<10){
        dummy1++;
    std::cout<<"r     : "<<std::hex<<r<<std::endl;
    std::cout<<"r_page: "<<std::hex<<(r>>12)<<std::endl;
    }
  auto set_end = std::find(begin, end, 0);
  auto found_reg = std::find(begin, set_end, r); // check to see if this register is already in the list
  *found_reg = r;
}
void ResetCurrentInstruction(VOID *ip, THREADID tid)
{
    if(tid!=champsim_trace_tid){
        return;
    }
    curr_instr = {};
    curr_instr.ip = (unsigned long long int)ip;
}
static VOID InsRef(ADDRINT addr, THREADID tid) //TODO add threaID to arg
{
	BOOL inROI_check = (inROI[tid]) || (inROI_master);
    if(!inROI_check){
    //if(!inROI[tid]){
        return;
    }
    // if(tid==champsim_trace_tid){
    //     curr_instr = {};
    //     curr_instr.ip = (unsigned long long int)addr;
    // }


    ins_count[tid]++;
    if(ins_count[tid] % 100000000==0){
    //if(ins_count[tid] % 1000==0){ // for testing
        if(ins_count[tid] % 1000000000==0){
            std::cout<<"thread_"<<tid << " ins_count: " << ins_count[tid] / 1000000000 << " B" << std::endl;
        }
        //Mark timestamp in the trace file
        dump_tbuf(tid);
        //fprintf(trace[tid], "INST_COUNT %ld\n", ins_count[tid]);
        uint64_t ts_signature=0xc0ffee;
        fwrite(&ts_signature, sizeof(uint64_t), 1,trace[tid]); //signature for inst count
        uint64_t tmp_inscount =ins_count[tid];
        fwrite(  &tmp_inscount, sizeof(uint64_t), 1,trace[tid]);

        //instrace write
        //fprintf(ins_trace[tid], "INST_COUNT %ld\n", ins_count[tid]);
    }

    const UINT32 size                        = 1; // assuming access does not cross cache lines
    const CACHE_BASE::ACCESS_TYPE accessType = CACHE_BASE::ACCESS_TYPE_LOAD;

    //// first level I-cache (Got rid of l1/l2 for data cache, keeping IL1
    //const BOOL il1Hit = il1.AccessSingleLine(addr, accessType);
    ADDRINT evicted_line=0; //unused for I cache
    const BOOL il1Hit = il1[tid]->AccessSingleLine(addr, accessType,evicted_line);
    //if (!il1Hit) Ul3Access(addr, size, accessType, tid);
    if (!il1Hit) Ul2Access(addr, size, accessType, tid, true);

}

static VOID MemRefMulti(ADDRINT addr, UINT32 size, CACHE_BASE::ACCESS_TYPE accessType, THREADID tid) //TODO add threaID to arg
{
	BOOL inROI_check = (inROI[tid]) || (inROI_master);
    if(!inROI_check){
    //if(!inROI[tid]){
        return;
    }
    //TODO figure out how to handle memref_multi
    //Ul3Access(addr, size, accessType, tid);
    Ul2Access(addr, size, accessType, tid);
    return;

    //memref_multi_count++;
    //if(memref_multi_count % 10000==0){
    //std::cout<<"memref_multi_count : "<<memref_multi_count/10000<<"*10k"<<std::endl;
    //}

    //fprintf(trace, "%p\n", (void *)addr);
    //return;

    ////TODO figure out how to handle memref_multi
    //for(UINT32 i=0;i<size;i++){
    //  ADDRINT addr_i = addr + i*64;
    //  fprintf(trace, "%p\n", (void *)addr_i);
    //}
    //return;
    ////above is test code. TODO REMOVE

}

static VOID MemRefSingle(ADDRINT addr, UINT32 size, CACHE_BASE::ACCESS_TYPE accessType, THREADID tid) //TODO add threaID to arg
{
	BOOL inROI_check = (inROI[tid]) || (inROI_master);
    if(!inROI_check){
        return;
    }

    //Ul3Access(addr, size, accessType, tid);
    Ul2Access(addr, size, accessType, tid);
    return;
    //DBG counter
    //memref_single_count++;
    //if(memref_single_count % 10000==0) {std::cout<<"memref_single_count : "<<memref_single_count/10000<<"*10k"<<std::endl;}
    //DBG for all accesses
    //fprintf(trace, "%p\n", (void *)addr);
    //return;


}

static VOID pin_magic_inst(THREADID tid, ADDRINT value, ADDRINT field){
        switch(field){
            case 0x0: //ROI START
                inROI[tid]=true;
                std::cout<<"ROI START (tid "<<tid<<")"<<std::endl;
                break;
            case 0x2: //ROI START_master
				inROI_master=true;
                std::cout<<"ROI START (MASTER)"<<std::endl;
                break;
            case 0x3: //ROI START (same as 0 but don't print anything
                inROI[tid]=true;
                //std::cout<<"ROI START (tid "<<tid<<")"<<std::endl;
                break;
            case 0x4: //ROI END
                inROI[tid]=false;
                //std::cout<<"ROI START (tid "<<tid<<")"<<std::endl;
                break;
            case 0x5: //Print Instruction count (dbg, setting FF, etc)
                std::cout<<"TRACER: MI-5 - Ins Count: "<<ins_count[tid]<<std::endl;
                break;
            default:
                break;

        }
        return;
}

static VOID Instruction(INS ins, VOID* v)
{
    //THREADID curtid = PIN_ThreadId();
    // all instruction fetches access I-cache
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)InsRef, IARG_INST_PTR, IARG_THREAD_ID, IARG_END);

    // MAGIC INSTRUCTIONS
    // start ROI if started 'FF'
    if (INS_IsXchg(ins) && INS_OperandReg(ins, 0) == REG_RBX && INS_OperandReg(ins, 1) == REG_RBX) {
        //std::cout<<"(xchg rbx rbx caught)!"<<std::endl;
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)pin_magic_inst, IARG_THREAD_ID, IARG_REG_VALUE, REG_RBX, IARG_REG_VALUE, REG_RCX, IARG_END);
        //INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ROI_start, IARG_THREAD_ID, IARG_END);
    }

    //getting champsim trace for just 1 thread
    //if(curtid==champsim_trace_tid){
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ResetCurrentInstruction, IARG_INST_PTR, IARG_THREAD_ID, IARG_END);
        if(INS_IsBranch(ins))
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)BranchOrNot, IARG_BRANCH_TAKEN, IARG_THREAD_ID, IARG_END);

        // instrument register reads
        UINT32 readRegCount = INS_MaxNumRRegs(ins);
        for(UINT32 i=0; i<readRegCount; i++) 
        {
            UINT32 regNum = INS_RegR(ins, i);
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)WriteToSet<unsigned char>,
                IARG_PTR, curr_instr.source_registers, IARG_PTR, curr_instr.source_registers + NUM_INSTR_SOURCES,
                IARG_UINT32, regNum, IARG_THREAD_ID, IARG_END);
        }

        // instrument register writes
        UINT32 writeRegCount = INS_MaxNumWRegs(ins);
        for(UINT32 i=0; i<writeRegCount; i++) 
        {
            UINT32 regNum = INS_RegW(ins, i);
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)WriteToSet<unsigned char>,
                IARG_PTR, curr_instr.destination_registers, IARG_PTR, curr_instr.destination_registers + NUM_INSTR_DESTINATIONS,
                IARG_UINT32, regNum, IARG_THREAD_ID, IARG_END);
        }

        // instrument memory reads and writes
        UINT32 memOperands = INS_MemoryOperandCount(ins);

        // Iterate over each memory operand of the instruction.
        for (UINT32 memOp = 0; memOp < memOperands; memOp++) 
        {
            if (INS_MemoryOperandIsRead(ins, memOp)) 
                INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)WriteToSet_mem<unsigned long long int>,
                    IARG_PTR, curr_instr.source_memory, IARG_PTR, curr_instr.source_memory + NUM_INSTR_SOURCES,
                    IARG_MEMORYOP_EA, memOp, IARG_THREAD_ID, IARG_END);
            if (INS_MemoryOperandIsWritten(ins, memOp)) 
                INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)WriteToSet_mem<unsigned long long int>,
                    IARG_PTR, curr_instr.destination_memory, IARG_PTR, curr_instr.destination_memory + NUM_INSTR_DESTINATIONS,
                    IARG_MEMORYOP_EA, memOp, IARG_THREAD_ID, IARG_END);
        }

        // finalize each instruction with this function
        INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)ShouldWrite,IARG_THREAD_ID, IARG_END);
        INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)WriteCurrentInstruction,IARG_THREAD_ID, IARG_END);
    //}

    if (!INS_IsStandardMemop(ins)) return;
    if (INS_MemoryOperandCount(ins) == 0) return;
    ;

    UINT32 readSize = 0, writeSize = 0;
    UINT32 readOperandCount = 0, writeOperandCount = 0;

    for (UINT32 opIdx = 0; opIdx < INS_MemoryOperandCount(ins); opIdx++)
    {
        if (INS_MemoryOperandIsRead(ins, opIdx))
        {
            readSize = INS_MemoryOperandSize(ins, opIdx);
            readOperandCount++;
            break;
        }
        if (INS_MemoryOperandIsWritten(ins, opIdx))
        {
            writeSize = INS_MemoryOperandSize(ins, opIdx);
            writeOperandCount++;
            break;
        }
    }

    if (readOperandCount > 0)
    {
        const AFUNPTR countFun = (readSize <= 4 ? (AFUNPTR)MemRefSingle : (AFUNPTR)MemRefMulti);

        // only predicated-on memory instructions access D-cache
        INS_InsertPredicatedCall(ins, IPOINT_BEFORE, countFun, IARG_MEMORYREAD_EA, IARG_MEMORYREAD_SIZE, IARG_UINT32,
                                 CACHE_BASE::ACCESS_TYPE_LOAD, IARG_THREAD_ID, IARG_END);
    }

    if (writeOperandCount > 0)
    {
        const AFUNPTR countFun = (writeSize <= 4 ? (AFUNPTR)MemRefSingle : (AFUNPTR)MemRefMulti);

        // only predicated-on memory instructions access D-cache
        INS_InsertPredicatedCall(ins, IPOINT_BEFORE, countFun, IARG_MEMORYWRITE_EA, IARG_MEMORYWRITE_SIZE, IARG_UINT32,
                                 CACHE_BASE::ACCESS_TYPE_STORE, IARG_THREAD_ID, IARG_END);
    }
}

VOID ThreadStart(THREADID tid, CONTEXT* ctxt, INT32 flags, VOID* v) {
    std::cout << "thread_" << tid<< " start" << std::endl;
    numThreads++;
    //std::string tfname = "memtrace_t" + std::to_string(tid) + ".out";
    std::ostringstream tfname;
    tfname << "memtrace_t" << tid << ".out";
    trace[tid] = fopen(tfname.str().c_str(), "wb");
    //trace_sancheck = fopen("asdf.txt", "w");

    std::ostringstream ins_tfname;
    ins_tfname << "ins_memtrace_t" << tid << ".out";
    //ins_trace[tid] = fopen(ins_tfname.str().c_str(), "w");

    if(KnobStartFF){
        inROI[tid]=false;
    }
    else{
        inROI[tid]=true;
    }


    il1[tid] = new IL1::CACHE("L1 Instruction Cache", IL1::cacheSize, IL1::lineSize, IL1::associativity);
    ul2[tid] = new UL2::CACHE("L2 Unified Cache", UL2::cacheSize, UL2::lineSize, UL2::associativity);
    ul3[tid] = new UL3::CACHE("L3 Unified Cache", UL3::cacheSize, UL3::lineSize, UL3::associativity);
}

extern int main(int argc, char* argv[])
{
    PIN_Init(argc, argv);

    if(KnobStartFF){
		std::cout<<"startFF"<<std::endl;
		inROI_master=false;
	}
	else{
		inROI_master=true;
	}
    champsim_skipins  = KnobSkipInstructions.Value();
    champsim_traceins = KnobTraceInstructions.Value();
    champsim_tracedoneins = champsim_skipins+champsim_traceins;
    std::cout<<"champsim skipins "<<champsim_skipins<<", champsim traceins: "<<champsim_traceins<<", champsim_tracedoneins: "<<champsim_tracedoneins<<std::endl;

    champsim_outfile.open("champsim.trace", std::ios_base::binary | std::ios_base::trunc);
    if (!champsim_outfile)
    {
      std::cout << "Couldn't open output trace file. Exiting." << std::endl;
        exit(1);
    }

    //trace = fopen(KnobOutputFile.Value().c_str(), "w");
    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);

    PIN_AddThreadStartFunction(ThreadStart, 0);
    PIN_AddThreadFiniFunction(ThreadFini, 0);

    std::cout<<"before calling prog"<<std::endl;
    // Never returns
    PIN_StartProgram();
    std::cout<<"never returns"<<std::endl;
    return 0; // make compiler happy
}
