#include "icn_sim.h"
#include <algorithm>
#include "champsim_constants.h"
#include "util.h"
#include "migrator.h"

#include "coh_directory.h"

extern uint8_t all_warmup_complete;
extern bool migration_all_done;
extern MIGRATOR mig0;
extern coh_directory coh_dir;

uint64_t dbg_instr_id = 10113396; //bfsbase003 dbg
uint64_t dbg_instr_id2 = 10086497; //FMIbase015 dbg
bool dbgpacket_in_UPITONL0=false;
uint64_t offending_insids_icn[5]={458781,1535278,1508815,1508043,454621};
bool is_offending_ins_icn(uint64_t insid){
  for(int i=0; i<5;i++){
    if(insid==offending_insids_icn[i]){
      return true;
    }
  }
  return false;
}



int ICN_SIM::add_rq(PACKET* packet) 
{
    if(is_offending_ins_icn(packet->instr_id)){
        cout<<"offending ins "<<packet->instr_id<<" in ICN_SIM"<<endl;
    }
    if (all_warmup_complete < NUM_CPUS) {
        for (auto ret : packet->to_return)
            ret->return_data(packet);

        return -1; // Fast-forward
    }
    
    packet_route pr = generate_packet_route(*packet, false);

    //dbg
    if(pr.packet.instr_id==dbg_instr_id){
        cout<<"add_rq after generate_pakcet_route"<<endl;
        cout<<"  num_links: "<<pr.num_links<<endl;
        cout<<"  occupancy: "<<lower_level->get_occupancy(1, packet->address)<<endl;
        cout<< " size:    : "<<lower_level->get_size(1, packet->address)<<endl;
    }

    // if src==dest, pass to mem right away
    if(pr.num_links==0){
        if (lower_level->get_occupancy(1, packet->address) 
            == lower_level->get_size(1, packet->address)) {
            // DRAM read q is full
            return -2;
        }
        return lower_level->add_rq(packet);
    }

    auto & ss_link = *(pr.links[0]);
    //bool qtype = pr.qtype[0]; //first qtype will always be REQ

    // check for the latest writebacks in the write queue
    auto found_wq = std::find_if(ss_link.respQ.begin(), ss_link.respQ.end(), 
                                eq_addr<packet_route>(packet->address, LOG2_BLOCK_SIZE));
    if (found_wq != ss_link.respQ.end()) {
        packet->data = found_wq->packet.data;
        for (auto ret : packet->to_return)
        ret->return_data(packet);
        return -1;
    }

    // check for duplicates in the read queue
    auto found_rq = std::find_if(std::begin(ss_link.reqQ), std::end(ss_link.reqQ), 
                                eq_addr<packet_route>(packet->address, LOG2_BLOCK_SIZE));
    if (found_rq != std::end(ss_link.reqQ)) {
        //channel.s_read_dup_merged++;
        packet_dep_merge(found_rq->packet.lq_index_depend_on_me, packet->lq_index_depend_on_me);
        packet_dep_merge(found_rq->packet.sq_index_depend_on_me, packet->sq_index_depend_on_me);
        packet_dep_merge(found_rq->packet.instr_depend_on_me, packet->instr_depend_on_me);
        packet_dep_merge(found_rq->packet.to_return, packet->to_return);

        //dbg - THis should never execute
        // if(packet->instr_id==0xc0ffee){
        //     std::cout<<"rq_merge happening for migration line"<<std::endl;
        //     merged_migration_lines++;
        //     std::cout<<"newaddr: "<<packet->address<<", merged_addr: "<<found_rq->packet.address<<std::endl;
        //     std::cout<<"new     Vaddr: "<<packet->v_address<<", src: "<<mig0.pages_to_migrate[packet->v_address>>LOG2_PAGE_SIZE].first << ", dest:"<<mig0.pages_to_migrate[packet->v_address>>LOG2_PAGE_SIZE].second <<std::endl;
        //     std::cout<<"merged  Vaddr: "<<found_rq->packet.v_address<<", src: "<<mig0.pages_to_migrate[found_rq->packet.v_address>>LOG2_PAGE_SIZE].first << ", dest:"<<mig0.pages_to_migrate[found_rq->packet.v_address>>LOG2_PAGE_SIZE].second <<std::endl;
        // }

        return 0; // merged index
    }

    int enq_retval = link_enq(pr, (&ss_link), false, REQ);
    //assert(enq_retval!=-2);
    //dbg
    if(pr.packet.instr_id==dbg_instr_id){
        cout<<" got to link_enq call, which returned: "<<enq_retval<<endl;
        cout<<" firstlink ssfull?: "<<ss_link.reqQ.full()<<endl;
        cout<<" firstlink occupancy: "<<ss_link.reqQ.occupancy()<<endl;
    }

    return enq_retval;

}

int ICN_SIM::add_wq(PACKET* packet) 
{
    if (all_warmup_complete < NUM_CPUS)
        return -1; // Fast-forward

    packet_route pr = generate_packet_route(*packet, true);
    if(pr.num_links==0){
         if (lower_level->get_occupancy(2, packet->address) 
            == lower_level->get_size(2, packet->address)) {
            // DRAM read q is full
            return -2;
        }
        return lower_level->add_wq(packet);
    }
    
    auto & ss_link = *(pr.links[0]);
    //bool qtype = pr.qtype[0]; //first qtype will always be REQ

    // check for duplicates in the write queue
    auto found_wq = std::find_if(ss_link.reqQ.begin(), ss_link.reqQ.end(), 
                                eq_addr<packet_route>(packet->address, LOG2_BLOCK_SIZE));
    if (found_wq != ss_link.reqQ.end()) {
        //should writes also call to-return?
        for (auto ret : packet->to_return)
            ret->return_data(packet);

        return 0; // merged index
    }

    // Check for room in the queue
    if (ss_link.reqQ.full()) {
        #if DBGTRACK
        ss_link.wq_full_count++;
        #endif
        return -2;
    }

    int enq_retval = link_enq(pr, (&ss_link), true, REQ);
    //assert(enq_retval!=-2);
    return enq_retval;

}

int ICN_SIM::add_pq(PACKET* packet) { return add_rq(packet); }

void ICN_SIM::return_data(PACKET* packet)
{    
    ///DBG/////TODO REMOVE
    U64 tmplat =  current_cycle-(packet->event_cycle);
    uint32_t dest=get_channel(packet->address);
    std::cout<<"Access took "<<tmplat<<"cycles, dest link: "<<dest<<std::endl;
    if(tmplat>1200){
        //uint32_t dest=get_channel(packet->address);
        std::cout<<"Access took "<<tmplat<<"cycles, dest link: "<<dest<<std::endl;
    }
    //I didn't add "THIS" to "to return", so ICN_SIM is probably bypassed on return path
    // so this porbably never gets executed
    packet->event_cycle = current_cycle;
    for(auto ret : packet->to_return){
        //TODO - should I pass &packet? check
        ret->return_data(packet);
    }

    //delays and all were handled before sending to DRAM
    // just return at this point
}

void ICN_SIM::operate() 
{
    s_cycles++;
    operate_delay_qs();
    
    handle_resps_and_reqs();

    //handle_resps();
    //handle_reqs();

    retire_bt_delay_q();
}

void ICN_SIM::retire_bt_delay_q(){
    while(bt_delayQ.has_ready()){
        PACKET& handle_pkt = bt_delayQ.front().packet;
        
        if(!handle_pkt.block_transfer){cout<<"WARNING retiring non block transfer from bt_delayQ?"<<endl;}
        uint64_t dest = bt_delayQ.front().dest;
        uint64_t time_in_icn = current_cycle - bt_delayQ.front().icn_entry_time;
        time_in_icn=time_in_icn-BT_DELAY; // remove BT_DELAY for stat tracking
        uint64_t hopcount = bt_delayQ.front().num_links;
        
        handle_pkt.event_cycle=current_cycle; // needed?
        for (auto ret : handle_pkt.to_return){ret->return_data(&handle_pkt);}
        bt_delayQ.pop_front();
        //stat tracking
        if(handle_pkt.cpu < NUM_CPUS){
            if(dest==CXO){
                cxl_bt_ret++;
                cxl_bt_latsum_ret+=time_in_icn;
                cxl_bt_hopsum+=hopcount;
            }
            else{
                non_cxl_bt_ret++;
                non_cxl_bt_latsum_ret+=time_in_icn;
                non_cxl_bt_hopsum+=hopcount;
            }
        }
        
    }
}

void ICN_SIM::operate_delay_qs() 
{
     for(auto& sslink_p : all_link_ptrs){ 
        auto& sslink=*sslink_p;
        sslink.reqQ.operate();
        sslink.respQ.operate();
        if(sslink.remaining_stall>0){
            sslink.remaining_stall--;
        }
     }
     bt_delayQ.operate();
}


int ICN_SIM::forward_packet_route(packet_route& pr, champsim::delay_queue<packet_route> & currQ){
    //return -2 if next link is full and cannot forward
    //otherwise, remove from current link, move to next link,
    //          and update pr.cur_link
    //          return size of retired packet from cur_link

    uint64_t new_link_count = pr.cur_link+1;
    PACKET& handle_pkt = pr.packet;
    bool isW = pr.isW;
    int rwflag=1;
    if(isW) rwflag=2;
    //bool curr_qtype = pr.qtype[pr.cur_link];
    bool next_qtype = pr.qtype[new_link_count];
    
    // retired packet size
    int packet_size = SMALL_PKT_SIZE;
    if(isW && (pr.qtype[pr.cur_link]==REQ)){packet_size=LARGE_PKT_SIZE;}
    if((!isW) && (pr.qtype[pr.cur_link]==RESP)){packet_size=LARGE_PKT_SIZE;}


    if(new_link_count<pr.num_links){ // send to next link
        //bool nextiscur=false;
        if(pr.links[pr.cur_link] == pr.links[pr.cur_link+1] ){
            cout<<"nextlink==curlink, does this happen?"<<endl;
            //nextiscur=true;
        }

        // champsim::delay_queue<packet_route> * currQ_ptr = &(pr.links[pr.cur_link]->reqQ);
        // if(curr_qtype==RESP){
        //     currQ_ptr = &(pr.links[pr.cur_link]->respQ);
        // }

        champsim::delay_queue<packet_route> * nextQ_ptr = &(pr.links[new_link_count]->reqQ);
        if(next_qtype==RESP){
            nextQ_ptr = &(pr.links[new_link_count]->respQ);
        }

        if(nextQ_ptr->full()){
            return -2;
        }
        //currQ_ptr->pop_front();
        currQ.pop_front();
        //** TODO - maybe logic for BW limit control should be here
       
        pr.cur_link=new_link_count;
        nextQ_ptr->push_back(pr);
        //pr.links[new_link_count]->allaccs++;
        pr.links[new_link_count]->all_traffic_in_B+=packet_size;

        return packet_size;

    }
    else{ //send to mem (OR call to_return if block-transfer)
        if(handle_pkt.block_transfer){ // block transfer- no further dest, return
            if(bt_delayQ.full()){
                cout<<"WARNING: bt_dealyQ full, return without it"<<endl;
                handle_pkt.event_cycle=current_cycle; // needed?
                for (auto ret : handle_pkt.to_return){ret->return_data(&handle_pkt);}
            }
            else{
                bt_delayQ.push_back(pr);                
            }            
            currQ.pop_front();
            return packet_size;
        }
        
        // can't send if memory rq/wq is full
        if (lower_level->get_occupancy(rwflag, handle_pkt.address) 
            == lower_level->get_size(rwflag, handle_pkt.address)) {
            // DRAM read q is full
            return -2;
        }
        
        // retire from icn and send to mem
        currQ.pop_front();
        if(isW){
            lower_level->add_wq(&handle_pkt);
        }
        else{
            lower_level->add_rq(&handle_pkt);
        }

        //some stat tracking
        uint64_t icn_lat_tmp = current_cycle - pr.icn_entry_time;
             
        //some new stat tracking.. clean up/coalesce with above sometime
        if(pr.packet.cpu<NUM_CPUS){
            if(pr.packet.instr_id==MIGRATION_INSID){
                if(pr.dest==CXO){
                    cxl_migration_ret++;
                    cxl_migration_latsum_ret+=icn_lat_tmp;
                }
                else{
                    non_cxl_migration_ret++;
                    non_cxl_migration_latsum_ret+=icn_lat_tmp;                    
                }                
            }
            else{
                if(pr.dest==CXO){
                    cxl_accs_ret++;
                    cxl_acc_latsum_ret+=icn_lat_tmp;
                    cxl_accs_hopsum+=pr.num_links;
                }
                else{
                    non_cxl_accs_ret++;
                    non_cxl_acc_latsum_ret+=icn_lat_tmp;
                    non_cxl_accs_hopsum+=pr.num_links;
                }
            }
        }



        return packet_size;
            
    }
    return 0;

}

bool ICN_SIM::check_nextlink_cantake(packet_route& pr){
    uint64_t new_link_count = pr.cur_link+1;
    PACKET& handle_pkt = pr.packet;
    bool isW = pr.isW;
    int rwflag=1;
    if(isW) rwflag=2;
    
    bool next_qtype = pr.qtype[new_link_count];
    
    if(new_link_count<pr.num_links){ // send to next link
        if(pr.links[pr.cur_link] == pr.links[pr.cur_link+1] ){
            cout<<"nextlink==curlink, does this happen? in check_nextolink_cantake"<<endl;
        }
        champsim::delay_queue<packet_route> * nextQ_ptr = &(pr.links[new_link_count]->reqQ);
        if(next_qtype==RESP){
            nextQ_ptr = &(pr.links[new_link_count]->respQ);
        }
        if(nextQ_ptr->full()){
            return false;
        }
        return true;
    }
    else{
        if (lower_level->get_occupancy(rwflag, handle_pkt.address) 
            == lower_level->get_size(rwflag, handle_pkt.address)) {
            // DRAM read q is full
            return false;
        }
        return true;
    }
    cout<<"code shouldn't get here"<<endl;
    return true; //won't get here
}

void ICN_SIM::handle_resps_and_reqs(){
    for(auto& sslink_p : all_link_ptrs){ 
        auto& sslink=*sslink_p;
        sslink.cur_accesses=0; //reset cur_accesses for this link (call only in this func)
        // TODO - 
        bool qtype = RESP;
        if(!(sslink.respQ.has_ready())){ qtype=REQ;}
        else if(!(sslink.reqQ.has_ready())){ qtype=RESP;}
        else {
            uint64_t reqFront_TS = sslink.reqQ.front().icn_entry_time;
            uint64_t respFront_TS = sslink.respQ.front().icn_entry_time;
            if(reqFront_TS>respFront_TS){ //RESP is older
                qtype=RESP;
            }
            else{
                if(check_nextlink_cantake(sslink.reqQ.front())){ // wanna check if nextlink for the REQ is blocked. set qtype=REQ only if it's not blocked
                    qtype=REQ;
                }
            }
        }
        champsim::delay_queue<packet_route> * qptr;
        qptr = &sslink.respQ;
        if(qtype==REQ){qptr=&sslink.reqQ;}
        //auto & curQ = sslink.respQ;
        auto & curQ = *qptr;
        while(curQ.has_ready()){
            if (sslink.starvation_check_reqQ > STARVATION_CHECK_LIM) {
                sslink.starvation_yields++;
                break; //avoid reqQ starvation
            }
            if(sslink.remaining_stall>0){
                break;
            }
            if(sslink.cur_accesses>=sslink.allowed_accesses){
                //this will probably never hit - current BW settings don't allow more than 1 packet per cycle
                // not only that, link has to wait multi cycles before sending next packet (above if statement)
                break;
            }
            packet_route& handle_pr=curQ.front();
            int fwp_retval = forward_packet_route(handle_pr, curQ);
            if(fwp_retval==-2){break;}
            sslink.cur_accesses++;
            // set stall till we can send next packet. Shorter delay if small msg
            // * sslink.bw_stall is defined for 64B packet (i.e. LARGE_PKT_SIZE)
            uint64_t divider = LARGE_PKT_SIZE / ((uint64_t) fwp_retval);
            assert(divider>0);
            sslink.remaining_stall=sslink.bw_stall/divider;
        }   
    }
}

void ICN_SIM::handle_resps(){
    for(auto& sslink_p : all_link_ptrs){ 
        auto& sslink=*sslink_p;
        sslink.cur_accesses=0; //reset cur_accesses for this link (call only in this func)
        
        auto & curQ = sslink.respQ;
        while(curQ.has_ready()){
            if (sslink.starvation_check_reqQ > STARVATION_CHECK_LIM) {
                sslink.starvation_yields++;
                break; //avoid reqQ starvation
            }
            if(sslink.remaining_stall>0){
                break;
            }
            if(sslink.cur_accesses>=sslink.allowed_accesses){
                //this will probably never hit - current BW settings don't allow more than 1 packet per cycle
                // not only that, link has to wait multi cycles before sending next packet (above if statement)
                break;
            }
            packet_route& handle_pr=curQ.front();
            int fwp_retval = forward_packet_route(handle_pr, curQ);
            if(fwp_retval==-2){break;}
            sslink.cur_accesses++;
            // set stall till we can send next packet. Shorter delay if small msg
            // * sslink.bw_stall is defined for 64B packet (i.e. LARGE_PKT_SIZE)
            uint64_t divider = LARGE_PKT_SIZE / ((uint64_t) fwp_retval);
            assert(divider>0);
            sslink.remaining_stall=sslink.bw_stall/divider;
        }   
    }
}
void ICN_SIM::handle_reqs(){
    for(auto& sslink_p : all_link_ptrs){ 
        auto& sslink=*sslink_p;
        
        auto & curQ = sslink.reqQ;
        while(curQ.has_ready()){
            if(sslink.remaining_stall>0){
                sslink.starvation_check_reqQ++;
                break;
            }
            if(sslink.cur_accesses>=sslink.allowed_accesses){
                //this will probably never hit - current BW settings don't allow more than 1 packet per cycle
                // not only that, link has to wait multi cycles before sending next packet (above if statement)
                sslink.starvation_check_reqQ++;
                break;
            }
            packet_route& handle_pr=curQ.front();
            int fwp_retval = forward_packet_route(handle_pr, curQ);
            if(fwp_retval==-2){break;}
            sslink.starvation_check_reqQ = 0; //retired a req, so reset starvation count
            sslink.cur_accesses++;
            // set stall till we can send next packet. Shorter delay if small msg
            // * sslink.bw_stall is defined for 64B packet (i.e. LARGE_PKT_SIZE)
            uint64_t divider = LARGE_PKT_SIZE / ((uint64_t) fwp_retval);
            assert(divider>0);
            sslink.remaining_stall=sslink.bw_stall/divider;
        }   
    }
}


uint32_t ICN_SIM::get_occupancy(uint8_t queue_type, uint64_t address) {
    cout<<"ICN_SIM::get_occupancy - SHOULD NOT BE CALLED"<<endl;
    return 0;
}

uint32_t ICN_SIM::get_size(uint8_t queue_type, uint64_t address) {
    cout<<"ICN_SIM::get_size - SHOULD NOT BE CALLED"<<endl;
    return 0;
}

//uint32_t ICN_SIM::get_occupancy(uint8_t queue_type, uint64_t address) {
uint32_t ICN_SIM::get_occupancy_icn(uint8_t queue_type, uint64_t address, PACKET hpkt) {
    //auto& channel = channels[get_channel(address)];
    uint32_t dest_node = get_channel(address);
    uint32_t source_node = address & 0xF; //up to 16 sockets, maxval is 15
    SS_LINK * ss_link;

    if(hpkt.cpu / NUM_CPUS != source_node){
        cout<<"WARNING: source_node != hpkt.cpu"<<endl;
    }

    
    if(source_node==dest_node){//if src==dest, check occupancy of mem
        if(hpkt.block_transfer==false){
            return lower_level->get_occupancy(queue_type, address);
        }
        else{
            if(source_node==hpkt.block_owner){
                cout<<"WARNING: src==dest==owner? instrID: "<<hpkt.instr_id<<", src: "<<source_node<<endl;
                return 0; //this shouldn't happen..
            }
            //block transfer with src==dest traverses the links like regular access from src(dest) to owner, 
            //so reassign variables for finding ss_link
            source_node=dest_node;
            dest_node=hpkt.block_owner;
        }
    }

    // *** Find first link and return reqQ occpuancy 

    //cout<<"get_occupancy ICNSIM - src node: "<<source_node<<", dest node: "<<dest_node<<", queue_type: "<<queue_type<<endl;
    if(dest_node==CXO){
        //cout<<"CXO"<<endl;
        ss_link = &(TO_CXL[source_node]);
    }
    else if(gethop(source_node, dest_node)==2){
        //cout<<"2hop"<<endl;
        ss_link = &(TO_REMOTE[source_node]);
    }
    else{ // has to be 1 hop
        ss_link = &(SS_LINKS[source_node][dest_node]);
    }

    return ss_link->reqQ.occupancy();
}

uint32_t ICN_SIM::get_size_icn(uint8_t queue_type, uint64_t address, PACKET hpkt)
{
    uint32_t dest_node = get_channel(address);
    uint32_t source_node = address & 0xF; //up to 16 sockets, maxval is 15
    
    SS_LINK * ss_link;


    if(source_node==dest_node){//if src==dest, check occupancy of mem
        if(hpkt.block_transfer==false){
            return lower_level->get_size(queue_type, address);
        }
        else{
            if(source_node==hpkt.block_owner){
                cout<<"WARNING: src==dest==owner? instrID: "<<hpkt.instr_id<<", src: "<<source_node<<endl;
                return 0; //this shouldn't happen..
            }
            //block transfer with src==dest traverses the links like regular access from src(dest) to owner, 
            //so reassign variables for finding ss_link
            source_node=dest_node;
            dest_node=hpkt.block_owner;
        }
    }

    // *** Find first link and return reqQ size 

    if(dest_node==CXO){
        //cout<<"CXO"<<endl;
        ss_link = &(TO_CXL[source_node]);
    }
    else if(gethop(source_node, dest_node)==2){
        //cout<<"2hop"<<endl;
        ss_link = &(TO_REMOTE[source_node]);
    }
    else{ // has to be 1 hop
        ss_link = &(SS_LINKS[source_node][dest_node]);
    }

    return ss_link->reqQ.size();

}

int invalid_chan=0;
uint32_t ICN_SIM::get_channel(uint64_t address) {
    // This returns the socket the houses the address, 
    //   but just keeping the function name as get_channel

    uint64_t cxl_bit = (address>>CXL_BIT & 1);
    if(cxl_bit==1){
        return CXO;
    }

    uint32_t channel = (address >> LOG2_PAGE_SIZE);
    channel = channel & (N_SOCKETS-1);
    assert(channel<N_SOCKETS);
    return channel;
}

bool ICN_SIM::can_take(uint64_t src, uint64_t dest, bool isW){
    //repeated calls to send_migration_lines is too slow,
    // hack around to check if ICN can take and skip action
    uint64_t hops = gethop(src, dest); 
    if(hops==2){
        // if(isW){
        //     return(!(TO_REMOTE[src].reqQ.full()));
        // }
        return(!(TO_REMOTE[src].reqQ.full()));
    }
    if(hops==1){
        // if(isW){
        //     return(!(SS_LINKS[src][dest].reqQ.full()));
        // }
        return(!(SS_LINKS[src][dest].reqQ.full()));
    }
    if(src==CXO){
        assert(dest!=CXO);
        // if(isW){
        //     return(!(FROM_CXL[dest].reqQ.full()));
        // }
        return(!(FROM_CXL[dest].reqQ.full()));
    }
    if(dest==CXO){
        assert(src!=CXO);
        // if(isW){
        //     return(!(TO_CXL[src].reqQ.full()));
        // }
        return(!(TO_CXL[src].reqQ.full()));
    }

    //code shouldn't get here
    std::cout<<"can_take - code shouldn't get here?"<<std::endl;
    return true;

}
packet_route ICN_SIM::generate_packet_route(PACKET packet, bool isW){
    packet_route pr;
    pr.packet=packet;
    pr.isW = isW;
    pr.address=packet.address;
    pr.icn_entry_time=current_cycle;
    pr.cur_link=0;

    pr.num_links=0;
    //uint64_t tmp_num_links=0;
    
    uint32_t dest_node = get_channel(packet.address);
	if((dest_node!=0) && (NUM_TBCPU==0)){
		cout<<"dest node: "<<dest_node<<endl;
	}
    pr.dest=dest_node;
    // NUM_CPUS per socket
    uint32_t source_node = packet.cpu / NUM_CPUS;
    uint32_t block_owner = packet.block_owner;
    bool src_s0 = (source_node == 0);

    //dbg
    if(pr.packet.instr_id==dbg_instr_id){
        cout<<"generate_packet_route, dbginstrID: "<<pr.packet.instr_id<<endl;
        cout<<"src: "<<source_node<<", dest: "<<dest_node<<", owner: "<<block_owner<<endl;
    }

    uint32_t routing_rule = 0;
    if(packet.block_transfer){
        n_block_transfers_icn++;
        if(src_s0){
            n_S0_block_transfers_icn++;
        }
        if(packet.block_owner==dest_node){routing_rule=0;}
        else if(dest_node==CXO){routing_rule=1;}
        else {routing_rule=2;}
    }
    
    if(routing_rule==0){ // no forwarding
        if(dest_node==CXO){
            n_CXO++;
            if(src_s0){
                S0_n_CXO++;
            }
        }
        else{
            uint64_t hops = gethop(source_node, dest_node); 
            if(hops==0) n_local++;
            if(hops==1) n_1hop++;
            if(hops==2) n_2hop++;
            if(src_s0){
                if(hops==0) S0_n_local++;
                if(hops==1) S0_n_1hop++;
                if(hops==2) S0_n_2hop++;
            }
        }
        uint32_t offset=0;
        uint32_t n_links_req  = add_links_to_pr(pr, source_node, dest_node, offset, REQ);
        uint32_t n_links_resp = add_links_to_pr(pr, dest_node, source_node, offset, RESP);
        assert(pr.num_links==(n_links_req+n_links_resp));
    }

    if(routing_rule==1){ //BLOCK FORWARDING for CXL
        assert(dest_node==CXO);
        n_homeCXL_but_block_transfer++;
        if(src_s0){n_S0_homeCXL_but_block_transfer++;}
        uint32_t offset=0;
        uint32_t n_links_req   = add_links_to_pr(pr, source_node, dest_node, offset, REQ);
        uint32_t n_links_req2  = add_links_to_pr(pr, dest_node, block_owner, offset, REQ);
        uint32_t n_links_resp  = add_links_to_pr(pr, block_owner, dest_node, offset, RESP);
        uint32_t n_links_resp2 = add_links_to_pr(pr, dest_node, source_node, offset, RESP);
        assert(pr.num_links==(n_links_req+n_links_req2+n_links_resp+n_links_resp2));
    }

    if(routing_rule==2){ //NON-CXL block forwarding
        assert(dest_node!=CXO);
        uint32_t offset=0;
        uint32_t n_links_req   = add_links_to_pr(pr, source_node, dest_node, offset, REQ);
        uint32_t n_links_req2  = add_links_to_pr(pr, dest_node, block_owner, offset, REQ);
        uint32_t n_links_resp  = add_links_to_pr(pr, block_owner, source_node, offset, RESP);
        //uint32_t n_links_resp2
        assert(pr.num_links==(n_links_req+n_links_req2+n_links_resp));

    }


    return pr;
}

uint32_t ICN_SIM::add_links_to_pr(packet_route &pr, uint32_t src, uint32_t dest, uint32_t & offset, bool qtype){
    //returns number of added links
    //if(src==dest){cout<<"ERROR: src==dest in add_links_to_pr"<<endl;}

    // ***** adding CXL link ***** //
    if(src==CXO){
        assert(dest!=CXO);
        pr.links[offset]=&(FROM_CXL[dest]);
        pr.qtype[offset]=qtype;
        pr.num_links=pr.num_links+1;
        offset++;
        return 1;
    }
    if(dest==CXO){
        assert(src!=CXO);
        pr.links[offset]=&(TO_CXL[src]);
        pr.qtype[offset]=qtype;
        pr.num_links=pr.num_links+1;
        offset++;
        return 1;
    }

    uint64_t hops = gethop(src, dest);
    if(hops==0){
        if (offset == 0) {
            // this code still gets called for any local accesses- returns 0
            // cout << "in add_links_to_pr, src==dest but offset==0. printing src, dest owner:" << endl;
            // cout << "src_arg: " << src << ", dest_arg: " << dest << ", block_owner: " << pr.packet.block_owner << endl;
        }
        return 0; //don't add any links. retire from icn_sim and send directly to mem
    }
    if(hops==1){
        pr.links[offset]=&(SS_LINKS[src][dest]);
        pr.qtype[offset]=qtype;
        pr.num_links=pr.num_links+1;
        offset++;
        return 1;
    }
    if(hops==2){
        pr.links[offset] = &(TO_REMOTE[src]);
        pr.links[offset+1]=&(SS_LINKS[src][dest]);
        pr.links[offset+2] = &(FROM_REMOTE[dest]);
        pr.qtype[offset]=qtype;
        pr.qtype[offset+1]=qtype;
        pr.qtype[offset+2]=qtype;
        pr.num_links=pr.num_links+3;
        offset=offset+3;
        return 3;
    }

    cout<<"WARNING - add_links_to_pr: code should not get here!"<<endl;
    return 0;
}


int ICN_SIM::link_enq(packet_route pr, SS_LINK * ss_l, bool isW, bool qtype){
    auto & ss_link = *(ss_l);
    
    //ss_link.allaccs++;
    int packet_size = SMALL_PKT_SIZE;
    if(isW && (pr.qtype[pr.cur_link]==REQ)){packet_size=LARGE_PKT_SIZE;}
    if((!isW) && (pr.qtype[pr.cur_link]==RESP)){packet_size=LARGE_PKT_SIZE;}
    ss_link.all_traffic_in_B+=packet_size;

    if(qtype==REQ){
        //assert(!ss_link.reqQ.full());
        if(ss_link.reqQ.full()){return -2;}
        ss_link.reqQ.push_back(pr);
        return ss_link.reqQ.occupancy();
    }
    else{
        assert(qtype==RESP);
        //assert(!ss_link.respQ.full());
        if(ss_link.respQ.full()){return -2;}
        ss_link.respQ.push_back(pr);
        return ss_link.respQ.occupancy();
    }

    return 0;
}

void ICN_SIM::ResetStats() {
    s_cycles = 0;
    //for (auto& channel: this.SS_LINKS) {
    //}
}

void ICN_SIM::PrintStats() {
    std::cout<<"ICN STATS"<<std::endl;
    std::cout<<"HOP COUNTS"<<std::endl;
    std::cout<<"n_2hop: "<<n_2hop<<std::endl;
    std::cout<<"n_1hop: "<<n_1hop<<std::endl;
    std::cout<<"n_local: "<<n_local<<std::endl;
    std::cout<<"n_CXO: "<<n_CXO<<std::endl;
    std::cout<<"n_block_transfers: "<<n_block_transfers_icn<<endl;
    std::cout<<"n_homeCXL_but_block_transfers: "<<n_homeCXL_but_block_transfer<<endl;
    std::cout<<"n_3way_block_transfers: "<<n_block_transfers_case2<<endl;

    std::cout<<"\nSOCKET 0 HOP COUNTS:"<<std::endl;
    std::cout<<"S0_n_2hop: "<<S0_n_2hop<<std::endl;
    std::cout<<"S0_n_1hop: "<<S0_n_1hop<<std::endl;
    std::cout<<"S0_n_local: "<<S0_n_local<<std::endl;
    std::cout<<"S0_n_CXO: "<<S0_n_CXO<<std::endl;
    std::cout<<"S0_n_block_trasnfers: "<<n_S0_block_transfers_icn<<endl;
    std::cout<<"S0_n_homeCXL_but_block_transfers: "<<n_S0_homeCXL_but_block_transfer<<endl;
    std::cout<<"S0_n_3way_block_transfers: "<<n_S0_block_transfers_case2<<endl;
    std::cout<<"\nSOCKET 0 stats taken at release from ICN:"<<std::endl;
    uint64_t alat_acc_cxl = 0;
    uint64_t alat_acc_non_cxl=0;
    uint64_t alat_bt_cxl = 0;
    uint64_t alat_bt_non_cxl = 0;
    uint64_t ahop_acc_cxl = 0;
    uint64_t ahop_acc_non_cxl=0;
    uint64_t ahop_bt_cxl = 0;
    uint64_t ahop_bt_non_cxl = 0;
    if(cxl_accs_ret>0){
        alat_acc_cxl = cxl_acc_latsum_ret/cxl_accs_ret;
        ahop_acc_cxl = cxl_accs_hopsum/cxl_accs_ret;
    }
    if(non_cxl_accs_ret>0){
        alat_acc_non_cxl = non_cxl_acc_latsum_ret/non_cxl_accs_ret;
        ahop_acc_non_cxl = non_cxl_accs_hopsum/non_cxl_accs_ret;
    }
    if(cxl_bt_ret>0){
        alat_bt_cxl = cxl_bt_latsum_ret / cxl_bt_ret;
        ahop_bt_cxl = cxl_bt_hopsum / cxl_bt_ret;
    }
    if(non_cxl_bt_ret>0){
        alat_bt_non_cxl = non_cxl_bt_latsum_ret / non_cxl_bt_ret;
        ahop_bt_non_cxl = non_cxl_bt_hopsum/ non_cxl_bt_ret;
    }
    std::cout<<"Average icn lat for cxl access: "<<alat_acc_cxl<<", ("<<cxl_accs_ret<<"), average links: "<< ahop_acc_cxl <<endl;
    std::cout<<"Average icn lat NON-cxl access: "<<alat_acc_non_cxl<<", ("<<non_cxl_accs_ret<<"), average links: "<<ahop_acc_non_cxl<<endl;
    std::cout<<"Average icn lat for cxl blktrs: "<<alat_bt_cxl<<", ("<<cxl_bt_ret<<"), average links: "<<ahop_bt_cxl<<endl;
    std::cout<<"Average icn lat NON-cxl blktrs: "<<alat_bt_non_cxl<<", ("<<non_cxl_bt_ret<<"), average links: "<<ahop_bt_non_cxl<<endl;
    uint64_t alat_all = 0;
    if((cxl_accs_ret+non_cxl_accs_ret+cxl_bt_ret+non_cxl_bt_ret)>0){
        alat_all = (non_cxl_acc_latsum_ret+cxl_acc_latsum_ret+non_cxl_bt_latsum_ret+cxl_bt_latsum_ret) / (cxl_accs_ret+non_cxl_accs_ret+cxl_bt_ret+non_cxl_bt_ret);
    }
    std::cout<<"Average icn lat for everything: "<<alat_all<<", ("<<(cxl_accs_ret+non_cxl_accs_ret+cxl_bt_ret+non_cxl_bt_ret) <<")"<<endl;
    std::cout<<"Migration lines stat(from socket 0 only)"<<endl;
    uint64_t alat_cxl_migration = 0;
    uint64_t alat_non_cxl_migration = 0;
    if(cxl_migration_ret>0){alat_cxl_migration = cxl_migration_latsum_ret / cxl_migration_ret;}
    if(non_cxl_migration_ret>0){alat_non_cxl_migration = non_cxl_migration_latsum_ret / non_cxl_migration_ret;}
    cout<<"migration to cxl average latency in icn: "<<alat_cxl_migration<<" ("<<cxl_migration_ret<<")"<<endl;
    cout<<"migration NONcxl average latency in icn: "<<alat_non_cxl_migration<<" ("<<non_cxl_migration_ret<<")"<<endl;

    //BW stats
    std::cout<<"ICN_BW: "<<std::endl;
    // numaccs * 64(B) / time, time=current_cycle / 2.4 ns
    uint64_t cur_t_ns = (10*current_cycle) / 24;
    // give unit in MB/s. numaccs*64B / cur_t_ns gives GB/s, so mult by 1000
    //let's just print for 4 cores as src
    std::cout<<"Socket-to-socket:"<<endl;
	uint64_t tmp_BW=0;
	uint64_t data_in_B=0;

    for(uint32_t ii=1; ii<N_SOCKETS;ii++){
        tmp_BW=0;
        data_in_B=SS_LINKS[0][ii].all_traffic_in_B;
        if(cur_t_ns>0){tmp_BW=data_in_B*1000 / cur_t_ns;}
        cout<<"SSL[0]["<<ii<<"] : "<<tmp_BW<<"MB/s"<<endl;
	 	tmp_BW=0;
	    data_in_B = SS_LINKS[ii][0].all_traffic_in_B;
	    if(cur_t_ns>0){tmp_BW=data_in_B*1000 / cur_t_ns;}
	    cout<<"SSL["<<ii<<"][0] : "<<tmp_BW<<"MB/s"<<endl;

    }
    //leaving loop commented out for potential future debug
    //uint64_t tmp_BW=0;
    //uint32_t ii = 10;
    //uint64_t data_in_B=SS_LINKS[0][ii].all_traffic_in_B;
    //if(cur_t_ns>0){tmp_BW=data_in_B*1000 / cur_t_ns;}
    //cout<<"SSL[0]["<<ii<<"] : "<<tmp_BW<<"MB/s"<<endl;
    //tmp_BW=0;
    //data_in_B = SS_LINKS[ii][0].all_traffic_in_B;
    //if(cur_t_ns>0){tmp_BW=data_in_B*1000 / cur_t_ns;}
    //cout<<"SSL["<<ii<<"][0] : "<<tmp_BW<<"MB/s"<<endl;

    tmp_BW=0;
    data_in_B=TO_REMOTE[0].all_traffic_in_B;
    if(cur_t_ns>0){tmp_BW=data_in_B*1000 / cur_t_ns;}
    std::cout<<"to Remote from S[0]: "<<tmp_BW<<"MB/s"<<endl;
    tmp_BW=0;
    data_in_B=FROM_REMOTE[0].all_traffic_in_B;
    if(cur_t_ns>0){tmp_BW=data_in_B*1000 / cur_t_ns;}
    std::cout<<"from Remote from S[0]: "<<tmp_BW<<"MB/s"<<endl;

	//dbg
	std::cout<<"DBG"<<endl;
    for(uint32_t ii=1; ii<N_SOCKETS;ii++){
		tmp_BW=0;
    	data_in_B=TO_REMOTE[ii].all_traffic_in_B;
    	if(cur_t_ns>0){tmp_BW=data_in_B*1000 / cur_t_ns;}
    	std::cout<<"to Remote from S["<<ii<<"]: "<<tmp_BW<<"MB/s"<<endl;
    	tmp_BW=0;
    	data_in_B=FROM_REMOTE[ii].all_traffic_in_B;
    	if(cur_t_ns>0){tmp_BW=data_in_B*1000 / cur_t_ns;}
    	std::cout<<"from Remote from S["<<ii<<"]: "<<tmp_BW<<"MB/s"<<endl;

	}

#if DBGTRACK
    std::cout<<"UPI to Remote Link from S[0] RQ full stall: "<<UPI_TO_NUMA_LINKS[0].rq_full_count<<endl;
    uint32_t avgocc=0;
    uint32_t avg_arrival_interval=0;
    if(UPI_TO_NUMA_LINKS[0].num_rq_enqs>0){
        avgocc=UPI_TO_NUMA_LINKS[0].sum_occupancy / UPI_TO_NUMA_LINKS[0].num_rq_enqs;
        avg_arrival_interval=UPI_TO_NUMA_LINKS[0].sum_interval / UPI_TO_NUMA_LINKS[0].num_rq_enqs;
    }
    std::cout<<"UPI to Remote Link from S[0] avg occupancy: "<<avgocc<<endl;
    std::cout<<"UPI to Remote Link from S[0] avg arrival interval: "<<avg_arrival_interval<<endl;
    std::cout<<"(UPI to Remote Link from S[0] enques: "<<UPI_TO_NUMA_LINKS[0].num_rq_enqs<<")"<<endl;
#endif
    tmp_BW=0;
    data_in_B=TO_CXL[0].all_traffic_in_B;
    if(cur_t_ns>0){tmp_BW=data_in_B*1000 / cur_t_ns;}
    std::cout<<"to CXL from S[0]: "<<tmp_BW<<"MB/s"<<endl;
    tmp_BW=0;
    data_in_B=FROM_CXL[0].all_traffic_in_B;
    if(cur_t_ns>0){tmp_BW=data_in_B*1000 / cur_t_ns;}
    std::cout<<"from CXL to S[0]: "<<tmp_BW<<"MB/s"<<endl;

#if DBGTRACK
    std::cout<<"to CXL from S[0] RQ full stall: "<<CXL_LINKS[0].rq_full_count<<endl;
    std::cout<<"(to CXL from S[0] enques : "<<CXL_LINKS[0].num_rq_enqs<<")"<<endl;
    avgocc=0;
    avg_arrival_interval=0;
    if(CXL_LINKS[0].num_rq_enqs>0){
        avgocc=CXL_LINKS[0].sum_occupancy / CXL_LINKS[0].num_rq_enqs;
        avg_arrival_interval=CXL_LINKS[0].sum_interval / CXL_LINKS[0].num_rq_enqs;
    }
    std::cout<<"to CXL from S[0] avg occupancy: "<<avgocc<<endl;
    std::cout<<"to CXL from S[0] avg arrival interval: "<<avg_arrival_interval<<endl;
#endif
    
}

void ICN_SIM::print_reqQ(SS_LINK &sslink, uint64_t & inflight_accs, uint64_t iter, uint64_t iter2 ){
            //inflight_accs+=sslink.reqQ.occupancy();
            //cout<<"sslink ["<<iter<<"]["<<iter2<<"] reqQ inflightaccs: "<<sslink.reqQ.occupancy()<<endl;
            cout<<"link front: cur_link: "<<sslink.reqQ.front().cur_link<<" / "<<sslink.reqQ.front().num_links<<endl;
            uint64_t dest_node = get_channel(sslink.reqQ.front().packet.address);
            cout<<"     src: "<<sslink.reqQ.front().packet.cpu<<", dest: " <<dest_node <<", block_owner: "<<sslink.reqQ.front().packet.block_owner;
            cout<<", addr: "<<sslink.reqQ.front().packet.address<<endl;
            cout<<"     nextlink reqQ occupancy: "<<sslink.reqQ.front().links[(sslink.reqQ.front().cur_link)+1]->reqQ.occupancy()<<endl;
            cout<<"     nextlink respQ occupancy: "<<sslink.reqQ.front().links[(sslink.reqQ.front().cur_link)+1]->respQ.occupancy()<<endl;
            cout << "     starvation_yields:  " << sslink.starvation_yields << endl;
            cout<<"     instr_id: "<<sslink.reqQ.front().packet.instr_id<<endl;
            cout<<"     cycle enqueued: "<<sslink.reqQ.front().packet.cycle_enqueued<<endl;
            cout<<"     icn_entry_time: "<<sslink.reqQ.front().icn_entry_time<<endl;
            cout<<"     time in ICN: "<< current_cycle - sslink.reqQ.front().icn_entry_time<<endl;
}

void ICN_SIM::print_respQ(SS_LINK &sslink, uint64_t & inflight_accs, uint64_t iter, uint64_t iter2 ){
    //inflight_accs+=sslink.respQ.occupancy();
    //cout<<"sslink ["<<iter<<"]["<<iter2<<"] respQ inflightaccs: "<<sslink.respQ.occupancy()<<endl;
    cout<<"link front: cur_link: "<<sslink.respQ.front().cur_link<<" / "<<sslink.respQ.front().num_links<<endl;
    uint64_t dest_node = get_channel(sslink.respQ.front().packet.address);
    cout<<"     src: "<<sslink.respQ.front().packet.cpu<<", dest: " <<dest_node <<", block_owner: "<<sslink.respQ.front().packet.block_owner;
    cout<<", addr: "<<sslink.respQ.front().packet.address<<endl;
    cout<<"     nextlink reqQ occupancy: "<<sslink.respQ.front().links[(sslink.respQ.front().cur_link)+1]->reqQ.occupancy()<<endl;
    cout<<"     nextlink respQ occupancy: "<<sslink.respQ.front().links[(sslink.respQ.front().cur_link)+1]->respQ.occupancy()<<endl;
    cout << "     starvation_yields:  " << sslink.starvation_yields << endl;
    cout<<"     instr_id: "<<sslink.respQ.front().packet.instr_id<<endl;
    cout<<"     cycle enqueued: "<<sslink.respQ.front().packet.cycle_enqueued<<endl;
    cout<<"     icn_entry_time: "<<sslink.respQ.front().icn_entry_time<<endl;
    cout<<"     time in ICN: "<< current_cycle - sslink.respQ.front().icn_entry_time<<endl;
}

void ICN_SIM::print_deadlock(){
    cout<<"ICN_SIM in-flight accs"<<std::endl;
    uint64_t inflight_accs=0;
    uint64_t iter=0;
    uint64_t iter2=0;

    for (auto& sslinkarr: SS_LINKS) {
      iter2=0;
      for(auto& sslink_p : sslinkarr){
        auto& sslink=sslink_p;
        if(sslink.reqQ.occupancy()>0){
            
            inflight_accs+=sslink.reqQ.occupancy();
            cout<<"sslink ["<<iter<<"]["<<iter2<<"] reqQ inflightaccs: "<<sslink.reqQ.occupancy()<<endl;
            print_reqQ(sslink, inflight_accs, iter,iter2 );
        }
        if(sslink.respQ.occupancy()>0){
            inflight_accs+=sslink.respQ.occupancy();
            cout<<"sslink ["<<iter<<"]["<<iter2<<"] respQ inflightaccs: "<<sslink.respQ.occupancy()<<endl;
            print_respQ(sslink, inflight_accs, iter,iter2 );
        }
        iter2++;
      }
      iter++;
    }
    iter=0;
    for(auto& sslink_p : TO_CXL){ 
        auto& sslink=sslink_p;
        if(sslink.reqQ.occupancy()>0){
            inflight_accs+=sslink.reqQ.occupancy();
            cout<<"sslink ["<<iter<<"]["<<iter2<<"] reqQ inflightaccs: "<<sslink.reqQ.occupancy()<<endl;
            print_reqQ(sslink, inflight_accs, iter,iter2 );
        }
        if(sslink.respQ.occupancy()>0){
            inflight_accs+=sslink.respQ.occupancy();
            cout<<"sslink ["<<iter<<"]["<<iter2<<"] respQ inflightaccs: "<<sslink.respQ.occupancy()<<endl;
            print_respQ(sslink, inflight_accs, iter,iter2 );
        }
        iter++;
    }
        iter=0;
    for(auto& sslink_p : FROM_CXL){ 
        auto& sslink=sslink_p;
        if(sslink.reqQ.occupancy()>0){
            inflight_accs+=sslink.reqQ.occupancy();
            cout<<"FROMCXL["<<iter<<"] reqQ inflightaccs: "<<sslink.reqQ.occupancy()<<endl;
            print_reqQ(sslink, inflight_accs, iter,iter2 );
        }
        if(sslink.respQ.occupancy()>0){
            inflight_accs+=sslink.respQ.occupancy();
            cout<<"FROMCXL["<<iter<<"] respQ inflightaccs: "<<sslink.respQ.occupancy()<<endl;
            print_respQ(sslink, inflight_accs, iter,iter2 );
        }
        iter++;
    }
    iter=0;
    for(auto& sslink_p : TO_REMOTE){ 
        auto& sslink=sslink_p;
        if(sslink.reqQ.occupancy()>0){
            inflight_accs+=sslink.reqQ.occupancy();
            cout<<"TO_REMOTE["<<iter<<"] reqQ inflightaccs: "<<sslink.reqQ.occupancy()<<endl;
           print_reqQ(sslink, inflight_accs, iter,iter2 );
        }
        if(sslink.respQ.occupancy()>0){
            inflight_accs+=sslink.respQ.occupancy();
            cout<<"TO_REMOTE["<<iter<<"] respQ inflightaccs: "<<sslink.respQ.occupancy()<<endl;
            print_respQ(sslink, inflight_accs, iter,iter2 );
        }
        iter++;
    }

       iter=0;
    for(auto& sslink_p : FROM_REMOTE){ 
        auto& sslink=sslink_p;
        if(sslink.reqQ.occupancy()>0){
            inflight_accs+=sslink.reqQ.occupancy();
            cout<<"FROM_REMOTE["<<iter<<"] reqQ inflightaccs: "<<sslink.reqQ.occupancy()<<endl;
            print_reqQ(sslink, inflight_accs, iter,iter2 );
        }
        if(sslink.respQ.occupancy()>0){
            inflight_accs+=sslink.respQ.occupancy();
            cout<<"FROM_REMOTE["<<iter<<"] respQ inflightaccs: "<<sslink.respQ.occupancy()<<endl;
            print_respQ(sslink, inflight_accs, iter,iter2 );
        }
        iter++;
    }

    cout<<"Accs in ICN_SIM: "<<inflight_accs<<endl;
    //cout<<"rr0s: "<<n_homeCXL_but_block_transfer<<endl;
    cout<<"rr1s: "<<n_homeCXL_but_block_transfer<<endl;
    cout<<"rr2s: "<< n_block_transfers_case2<<endl;
}
