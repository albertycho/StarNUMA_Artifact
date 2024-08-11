#ifndef __HMEM__H
#define __HMEM__H

#include "champsim_constants.h"
#include "memory_class.h"
#include "operable.h"
#include "dramsim3_wrapper.hpp"
#include "util.h"
#include <iostream>

extern uint8_t all_warmup_complete;

class HMEM: public champsim::operable, public MemoryRequestConsumer
{
public:
    HMEM(double freq_scale, const std::string& config_file,const std::string& config_file_CXL, const std::string& output_dir):
        champsim::operable(freq_scale), 
        MemoryRequestConsumer(std::numeric_limits<unsigned>::max()),
        base_dram(freq_scale, config_file, output_dir),
        cxl_device(freq_scale, config_file_CXL, output_dir) 
        {
            //base_dram = DRAMSim3_DRAM(freq_scale, config_file, output_dir);
            std::cout << "Base_DRAM init" << std::endl;   
            std::cout << "CXL_DEVICE init" << std::endl;  
			//memory_system_->setCPUClockSpeed(2000000000); //set to 2ghz
        }

    DRAMSim3_DRAM base_dram;
    DRAMSim3_DRAM cxl_device;

    
    bool cxladdr(uint64_t address){
        uint64_t res = (address>>CXL_BIT) & 1;
        return res==1;
    }
    int add_rq(PACKET* packet) override {
        if (all_warmup_complete < NUM_CPUS) {
            for (auto ret : packet->to_return)
                ret->return_data(packet);

            return -1; // Fast-forward
        }
        if(packet->block_transfer){
            std::cout<<"WARN: block transfer reached hybrid_mem_wrapper! insID: "<<packet->instr_id<<std::endl;
        }
        //assert(packet->block_transfer==false);
        bool cxlacc = cxladdr(packet->address);
        if(cxlacc){
            return cxl_device.add_rq(packet);
        }
        else{
            return base_dram.add_rq(packet);
        }
    }
    int add_wq(PACKET* packet) override {
        if (all_warmup_complete < NUM_CPUS){
            for (auto ret : packet->to_return)
                ret->return_data(packet);

            return -1; // Fast-forward
        }
        
        bool cxlacc = cxladdr(packet->address);
        if(cxlacc){
            return cxl_device.add_wq(packet);
        }
        else{
            return base_dram.add_wq(packet);
        }
    }
    int add_pq(PACKET* packet) override {
        return add_rq(packet);
    }

    void operate() override {
        base_dram.operate();
        cxl_device.operate();
    }

    uint32_t get_occupancy(uint8_t queue_type, uint64_t address) override {
        bool cxlacc = cxladdr(address);
        if(cxlacc){
            return cxl_device.get_occupancy(queue_type,address);
        }
        else{
            return base_dram.get_occupancy(queue_type,address);
        }
    }
    uint32_t get_size(uint8_t queue_type, uint64_t address) override {
        bool cxlacc = cxladdr(address);
        if(cxlacc){
            return cxl_device.get_size(queue_type,address);
        }
        else{
            return base_dram.get_size(queue_type,address);
        }
    }


    void PrintStats() { 
        base_dram.PrintStats(); 
        cxl_device.PrintStats();
    }
};

#endif
