#ifndef __DRAMSIM3__H
#define __DRAMSIM3__H

#include "champsim_constants.h"
#include "memory_class.h"
#include "operable.h"
#include "dramsim3.h"
#include "util.h"

namespace dramsim3 {
    class MemorySystem;
};

extern uint8_t all_warmup_complete;

class DRAMSim3_DRAM: public champsim::operable, public MemoryRequestConsumer
{
public:
    DRAMSim3_DRAM(double freq_scale, const std::string& config_file, const std::string& output_dir):
        champsim::operable(freq_scale), 
        MemoryRequestConsumer(std::numeric_limits<unsigned>::max()) {
            memory_system_ = new dramsim3::MemorySystem(config_file, output_dir,
                                            std::bind(&DRAMSim3_DRAM::ReadCallBack, this, std::placeholders::_1),
                                            std::bind(&DRAMSim3_DRAM::WriteCallBack, this, std::placeholders::_1));
            std::cout << "DRAMSim3_DRAM init" << std::endl;   
			//memory_system_->setCPUClockSpeed(2000000000); //set to 2ghz
        }

    // This is a wrapper so DRAMSim (which only returns trans. addr) can communicate
    // with ChampSim API (which requires explicit packert->to_return->return_data calls)
    // We maintain a "Meta-RQ" and "Meta-WQ" to keep track of duplicates and mergeable reqs
    int add_rq(PACKET* packet) override {
        if (all_warmup_complete < NUM_CPUS) {
            for (auto ret : packet->to_return)
                ret->return_data(packet);

            return -1; // Fast-forward
        }
        
        // Check for forwarding
        // Returns data without transacting with DRAMSim
        auto wq_it = std::find_if(std::begin(WQ), std::end(WQ), 
                                    eq_addr<PACKET>(packet->address, LOG2_BLOCK_SIZE));
        if (wq_it != std::end(WQ)) {
            packet->data = wq_it->data;
            for (auto ret : packet->to_return)
                ret->return_data(packet);
            return -1; // merged index
        }

        // Check for duplicates
        // Does not add a new transaction to DRAMSim
        auto rq_it = std::find_if(std::begin(RQ), std::end(RQ), 
                                    eq_addr<PACKET>(packet->address, LOG2_BLOCK_SIZE));
        if (rq_it != std::end(RQ)) {
            packet_dep_merge(rq_it->lq_index_depend_on_me, packet->lq_index_depend_on_me);
            packet_dep_merge(rq_it->sq_index_depend_on_me, packet->sq_index_depend_on_me);
            packet_dep_merge(rq_it->instr_depend_on_me, packet->instr_depend_on_me);
            packet_dep_merge(rq_it->to_return, packet->to_return);

            return std::distance(std::begin(RQ), rq_it); // merged index
        }
        
        // Find empty slot
        rq_it = std::find_if_not(std::begin(RQ), std::end(RQ), is_valid<PACKET>());
        if (rq_it == std::end(RQ) || memory_system_->WillAcceptTransaction(packet->address, false) == false) {
			std::cout<<"[PANIC] RQ cannot accept entries or DRAMSim3 cannot accept transaction!"<<std::endl;
            assert(0); // This should not happen as we check occupancy before calling add_rq
        }
        // Call to DRAMSim
        memory_system_->AddTransaction(packet->address, false);
		//uint64_t t_before_addtransaction = current_cycle-packet->cycle_enqueued;
		//std::cout<<"t_before_addtransaction:"<< t_before_addtransaction<<std::endl;
		//std::cout<<"t_enq:"<< packet->cycle_enqueued<<std::endl;
        // Add to RQ
        // Remember this packet to later return data
        *rq_it = *packet;
        rq_it->event_cycle = current_cycle;

        return get_occupancy(1, packet->address);
    }
    int add_wq(PACKET* packet) override {
        if (all_warmup_complete < NUM_CPUS){
            for (auto ret : packet->to_return)
                ret->return_data(packet);

            return -1; // Fast-forward
        }


        // Check for duplicates
        // Does not add a new transaction to DRAMSim
        auto wq_it = std::find_if(std::begin(WQ), std::end(WQ), eq_addr<PACKET>(packet->address, LOG2_BLOCK_SIZE));
        if (wq_it != std::end(WQ))
            return 0;

        // search for the empty index
        wq_it = std::find_if_not(std::begin(WQ), std::end(WQ), is_valid<PACKET>());
        // If Meta-WQ is full or DRAMSim3 mem-system is busy, return
        if (wq_it == std::end(WQ) || memory_system_->WillAcceptTransaction(packet->address, true) == false) {
            return -2;
        }
        // Call to DRAMSim
        memory_system_->AddTransaction(packet->address, true);

        // Add to WQ
        // Remember this packet to later return data
        *wq_it = *packet;
        wq_it->event_cycle = current_cycle;

        return get_occupancy(2, packet->address);
    }
    int add_pq(PACKET* packet) override {
        return add_rq(packet);
    }

    void operate() override {
        memory_system_->ClockTick();
    }

    uint32_t get_occupancy(uint8_t queue_type, uint64_t address) override {
        if (queue_type == 1) {
            uint64_t rq_occ = std::count_if(std::begin(RQ), std::end(RQ), is_valid<PACKET>());
            if (memory_system_->WillAcceptTransaction(address, false) == false) {
                // DRAMSim cannot accept transaction, this addr must not be inserted
                rq_occ = RQ.size();
            }
            return rq_occ;
        }
        else if (queue_type == 2) {
            uint64_t wq_occ = std::count_if(std::begin(WQ), std::end(WQ), is_valid<PACKET>());
            if (memory_system_->WillAcceptTransaction(address, true) == false) {
                // DRAMSim cannot accept transaction, this addr must not be inserted
                wq_occ = WQ.size();
            }
            return wq_occ;
        }
        else if (queue_type == 3)
            return get_occupancy(1, address);

        return -1;        
    }
    uint32_t get_size(uint8_t queue_type, uint64_t address) override {
        if (queue_type == 1)
            return RQ.size();
        else if (queue_type == 2)
            return WQ.size();
        else if (queue_type == 3)
            return get_size(1, address);
        return -1;
    }

    void ReadCallBack(uint64_t addr) { 
        auto rq_pkt = std::find_if(std::begin(RQ), std::end(RQ), 
                                    eq_addr<PACKET>(addr, LOG2_BLOCK_SIZE));
        if (rq_pkt != std::end(RQ)) {
            rq_pkt->event_cycle = current_cycle;
            //std::cout<<"dramsimwrapper readcallback. addr:"<< rq_pkt->address <<" ret_cycle:"<<rq_pkt->event_cycle<<std::endl;
            //rq_pkt->event_cycle = current_cycle+80; //add ~30ns . CXL temp WA!! TODO REMOVE
            //std::cout<<"CXL adjusted ret_cycle:"<<rq_pkt->event_cycle<<std::endl;
            //std::cout<<"dramsimwrapper readcallback. addr:"<< rq_pkt->address <<" ret_cycle:"<<rq_pkt->event_cycle<<std::endl;
            for (auto ret : rq_pkt->to_return) 
                ret->return_data(&(*rq_pkt));
            *rq_pkt = {};
        }
        else {
            std::cout << "[PANIC] RQ packet not found on DRAMSim req completion! Exiting..." 
                        << std::endl;
            assert(0);
        }
    }
    void WriteCallBack(uint64_t addr) { 
        auto wq_pkt = std::find_if(std::begin(WQ), std::end(WQ), 
                                    eq_addr<PACKET>(addr, LOG2_BLOCK_SIZE));
        if (wq_pkt != std::end(WQ)) {
            wq_pkt->event_cycle = current_cycle;
            //wq_pkt->event_cycle = current_cycle+80; //CXL TEMP WA!!! TODO REMOVE
            for (auto ret : wq_pkt->to_return) 
                ret->return_data(&(*wq_pkt));
            *wq_pkt = {};
        }
        else {
            std::cout << "[PANIC] WQ packet not found on DRAMSim req completion! Exiting..." 
                        << std::endl;
            assert(0);
        }
    }
    void PrintStats() { memory_system_->PrintStats(); }
protected:
    dramsim3::MemorySystem* memory_system_;
    std::vector<PACKET> WQ{DRAM_WQ_SIZE*DRAM_CHANNELS}; // Meta-WQ
    std::vector<PACKET> RQ{DRAM_RQ_SIZE*DRAM_CHANNELS}; // Meta-RQ
};

#endif
