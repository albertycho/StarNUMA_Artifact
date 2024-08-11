#include "coh_directory.h"
#include <assert.h>
#include <iostream>
#include "tb_cpu.h"
#include "cache.h"
#include <random>
#include <vector>

extern tb_cache_t tb_llcs[N_SOCKETS];
extern std::array<CACHE*, NUM_CACHES> caches;

extern uint8_t all_warmup_complete;

uint64_t offending_lineaddr = 1215854624;

// return true if access requires block transfer
int coh_directory::get_coh_owner(uint64_t addr, uint64_t cpu_id, bool isW){
    int retval = -1;
    uint64_t socket_id = cpu_id / NUM_CPUS;
    uint64_t lineaddr = addr>>LOG2_BLOCK_SIZE;
    coh_dir_entry &dentry = directory[lineaddr];

    if((dentry.state==S) && (dentry.sharers[socket_id]==false)){
        retval = dentry.owner;
        if(retval>=N_SOCKETS){
            std::cout<<"entry is in S but does not have valid owner"<<endl;
            retval = -1;
        }
    }

    switch(dentry.state){
        case I:
            retval = -1;
            break;
        case M:
        case E:
            if(dentry.sharers[socket_id]){
                retval = -1;
            }
            else{
                retval = dentry.owner;
                if(retval>=N_SOCKETS){
                    std::cout<<"entry is in S but does not have valid owner"<<endl;
                    retval = -1;
                }
            }
            break;
        case S:
            if(dentry.sharers[socket_id]){
                retval = -1;
                if(isW){ // send inval
                    //retval=true;
                    retval = dentry.owner;
                    if(retval==(int)socket_id){
                        //cout<<"sending INVAL: src==owner, so returning a different sharer as owner"<<endl;
                        std::vector<int> trueIndices;
                        for(int i = 0; i < N_SOCKETS; i++) {
                            if(dentry.sharers[i]) {
                                if(i!=(int) socket_id){
                                    trueIndices.push_back(i);
                                }
                            }
                        }
                        if (!trueIndices.empty()) {
                            int rindex=rand() % trueIndices.size();
                            retval=trueIndices[rindex];
                            if(retval>=N_SOCKETS){cout<<"invalid owner for sending inval"<<endl;}
                            if(retval==(int)socket_id){cout<<"reassigned owenr but still src==owner"<<endl;}
                        }
                    }
                    if(retval>=N_SOCKETS){
                        std::cout<<"entry is in S but does not have valid owner"<<endl;
                        retval = -1;
                    }
                }
            }
            else{ // same as if block is in cache. if R, will just go to mem.
                  // if W, has to send inval
                retval = -1;
                if(isW){ // send inval
                    //retval=true;
                    retval = dentry.owner;
                    if(retval==(int)socket_id){
                        //cout<<"sending INVAL: src==owner, so returning a different sharer as owner"<<endl;
                        std::vector<int> trueIndices;
                        for(int i = 0; i < N_SOCKETS; i++) {
                            if(dentry.sharers[i]) {
                                if(i!=(int) socket_id){
                                    trueIndices.push_back(i);
                                }
                            }
                        }
                        if (!trueIndices.empty()) {
                            int rindex=rand() % trueIndices.size();
                            retval=trueIndices[rindex];
                            if(retval>=N_SOCKETS){cout<<"invalid owner for sending inval"<<endl;}
                            if(retval==(int)socket_id){cout<<"reassigned owenr but still src==owner"<<endl;}
                        }
                    }
                    if(retval>=N_SOCKETS){
                        std::cout<<"entry is in S but does not have valid owner"<<endl;
                        retval = -1;
                    }
                }

                
            }
    }
    
    update_block(addr, cpu_id, isW);

    if(retval!=-1){
        if (all_warmup_complete < NUM_CPUS) {
            n_block_transfers++;
            if(socket_id==0){
                n_block_transfers_S0++;
            }
        }
    }

    return retval;
}
void coh_directory::update_block(uint64_t addr, uint64_t cpu_id, bool isW){
    uint64_t socket_id = cpu_id / NUM_CPUS;
    uint64_t lineaddr = addr>>LOG2_BLOCK_SIZE;
    coh_dir_entry &dentry = directory[lineaddr];
    
    //TODO this is just dummy code - UPDATE IT!!!!      
    //dentry.owner=socket_id;
    dentry.sharers[socket_id]=true;

    uint64_t cur_sharers=0;
    //uint64_t potential_new_owner=N_SOCKETS;
    for(uint64_t i=0; i<N_SOCKETS;i++){
        if(dentry.sharers[i]){
            cur_sharers++;
            //potential_new_owner = i;
        }
    }

    if(isW){
        dentry.state=M;
        dentry.owner=socket_id;
        uint64_t n_invals = 0;
        //TODO - unset all other sharers
        for(uint64_t i=0; i<N_SOCKETS;i++){
            if(i!=socket_id){
                if(dentry.sharers[i]){
                    dentry.sharers[i]=false;
                    if(i==0){
                        //TODO invalidate caches on timing core
                        uint64_t s0invals=send_inval_to_s0_caches(addr);
                        if(s0invals==0){cout<<"WARNING - attempted to inval s0 caches but did not have entry"<<endl;}
                        else{
                            n_invals++;
                            n_invals_to_s0++;
                        }
                    }
                    else{
                        //TODO invalidate tb_llc 
                        inval_tb_cache(tb_llcs[i], lineaddr, i);
                        n_invals++;
                    }
                }
            }            
        }
        dentry.sharers[socket_id]=true; //re-set current accessing node
        if(n_invals>0){
            n_inval_event++;
            n_inval_msgs+=n_invals;
        }
    }
    else{ //READ
        if(cur_sharers>1){
            dentry.state=S;
        }
        else{
            //single owner. state is E, unless 
            if(dentry.state!=M){
                dentry.state=E;
                dentry.owner=socket_id;
            }
        }
    }

    //DBG
    // if(lineaddr==offending_lineaddr){
    //     cout<<"offending lineaddr is accessed. isW: "<<isW<<endl;
    // }
    
}

void coh_directory::update_evict(uint64_t addr, uint64_t cpu_id, std::string caller_name){
    
    update_evict_calls++;

    uint64_t socket_id = cpu_id / NUM_CPUS;
    uint64_t lineaddr = addr>>LOG2_BLOCK_SIZE;
    coh_dir_entry &dentry = directory[lineaddr];


    //DBG
    // if(lineaddr==offending_lineaddr){
    //     cout<<"offending lineaddr is evicted. state: "<< dentry.state<<endl;
    //     cout<<"caller: "<<caller_name<<endl;
    // }

    if(!(dentry.sharers[socket_id])){
        cout<<"ERROR - attempted to unset directory.sharer bit after eviction, but bit already unset"<<endl;
        cout<<"offending caller: "<<caller_name<<endl;
        cout<<"current directory size: "<<directory.size()<<endl;
        cout<<"calls to update_evict so far: "<<update_evict_calls<<endl;
        cout<<"offending lineaddr: "<<lineaddr<<endl;       
        cout<<"offending addr: "<<addr<<endl;       

		reset_broken_block(addr);
    }
    //assert(dentry.sharers[socket_id]);
    dentry.sharers[socket_id]=false;

    uint64_t cur_sharers=0;
    uint64_t potential_new_owner=N_SOCKETS;
    for(uint64_t i=0; i<N_SOCKETS;i++){
        if(dentry.sharers[i]){
            cur_sharers++;
            if(potential_new_owner==N_SOCKETS){
                potential_new_owner = i;
            }
            else{
                if(rand()%2==0){
                    potential_new_owner = i;
                }
            }
        }
    }

    switch(dentry.state){
        case I:
        //should not be I
            break;
        case S:
            if(dentry.owner==socket_id){dentry.owner=potential_new_owner;}
            if(cur_sharers==1){
                dentry.state=E;
            }
            if(cur_sharers==0){
                std::cout<<"previous state was S, but cur_sharers are 0"<<std::endl;
            }
            break;
        case E:
            if(cur_sharers>0){std::cout<<"state was E but there were other sharers"<<std::endl;}
			else{directory.erase(lineaddr); }
            dentry.state=I;
            dentry.owner=N_SOCKETS;
            break;
        case M:
            if(cur_sharers>0){std::cout<<"state was M but there were other sharers"<<std::endl;}
			else{directory.erase(lineaddr); }
            dentry.state=I;
            dentry.owner=N_SOCKETS;
            break;
        
    }



}



uint64_t coh_directory::send_inval_to_s0_caches(uint64_t addr){
    uint64_t retval = 0;
    for (CACHE* cache : caches) {
        bool not_tlb=false;
        // invalidate cahces that are not "TLB"
        if (cache->NAME.find("TLB") == std::string::npos){ not_tlb=true;}
        if(not_tlb){
            int inval_hit = cache->invalidate_entry(addr);
            //cache->invalidate_entry(addr);
            if(inval_hit!=-1){retval++;}
        }
    }
    return retval;
}

//reset block if coherence state and cache states don't check out
uint64_t coh_directory::reset_broken_block(uint64_t addr){
    reset_broken_block_count++;

    //Option 1 - invalidate everything
    send_inval_to_s0_caches(addr);
    uint64_t lineaddr = addr>>LOG2_BLOCK_SIZE;
    directory[lineaddr].sharers[0]=0;
    
    for(uint64_t i=1; i<N_SOCKETS;i++){
        inval_tb_cache(tb_llcs[i], lineaddr, i);
        directory[lineaddr].sharers[i]=0;
    }


    //Option 2 -
    //  0) if coh_dir state is currently set as I, go ahead and blindly invalidate everything
    //  1) check if line exists in socket 0, and set coh_dir.sharers[0] accordingly
    //      if dirty is set, set state to M and invalidate/unset sharer for all other sockets
    //  2) otherwise, check cache and set sharers for all other sockets
    //      unset dirty bits in other sockets if set
    //  3) if 2 or more sharers, set state to S. else E
    return 0;
}


void coh_directory::debug_print(){
    std::cout<<"coh_directory dbg print:"<<endl;
    std::cout<<"num directory entries: "<<directory.size()<<endl;
    uint64_t Mc=0;
    uint64_t Ec=0;
    uint64_t Sc=0;
    uint64_t Ic=0;
    for (const auto& entry : directory) {
        //uint64_t key = entry.first;
        coh_dir_entry cdentry = entry.second;
        switch(cdentry.state){
            case M:
                Mc++;
                break;
            case E:
                Ec++;
                break;
            case S:
                Sc++;
                break;
            case I:
                Ic++;
                break;
        }
    }
    cout<<"Ms:" <<Mc<<endl;
    cout<<"Es:" <<Ec<<endl;
    cout<<"Ss:" <<Sc<<endl;
    cout<<"Is:" <<Ic<<endl;
    cout<<"block_transfers: "<<n_block_transfers<<endl;
    cout<<"block_transfers to socket0: "<<n_block_transfers_S0<<endl;
    cout<<"inval events: "<<n_inval_event<<", total inval msgs: "<<n_inval_msgs;
    cout<<", invals to s0: "<<n_invals_to_s0<<endl;
    cout<<"update_evict calls: "<<update_evict_calls<<endl;
    // cout<<"evict but update_evict not called: "<<evict_but_update_evict_not_called<<endl;
    // cout<<"evict but update_evict not called from LLC: "<<evict_calls_from_LLC<<endl;
    // cout<<"evict but update_evict not called from L1L2: "<<evict_calls_from_l1l2<<endl;
    cout<<"reset broken blocks: "<<reset_broken_block_count<<endl;
}
