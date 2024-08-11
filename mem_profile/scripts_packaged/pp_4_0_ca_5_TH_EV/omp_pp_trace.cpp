#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <map>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <stdint.h>
#include <limits.h>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <chrono>
#include <cmath>
#include <queue>
#include <stack>
#include <set>
#include <unordered_map>
#include <omp.h>
#include <sys/stat.h> 
#include <sys/types.h> 
#include "omp_pp_trace.hpp"


// #define PAGESIZE 4096
// #define PAGEBITS 12
// #define N_THR 16
// //#define THR_OFFSET_VAL 2
// #define THR_OFFSET_VAL 0
// #define N_THR_OFFSET (N_THR+1)
// #define U64 uint64_t
// #define CXO 100 //CXL Island is owner
// #define SHARER_THRESHOLD 8
// #define INVAL_OWNER 9999
// #define PHASE_CYCLES 1000000000

#define LIMIT_MIGRATION 1
//#define MIGRATION_LIMIT 12288


using namespace std;

FILE * trace[N_THR];

omp_lock_t page_W_lock;
omp_lock_t page_R_lock;
//omp_lock_t page_owner_lock;
//omp_lock_t page_owner_CI_lock;
//omp_lock_t hop_hist_lock;
//omp_lock_t hop_hist_CI_lock;

vector<unordered_map<uint64_t, uint64_t>> page_access_counts_dummy;
// vector<unordered_map<uint64_t, uint64_t>> page_access_counts_R(N_THR);
// vector<unordered_map<uint64_t, uint64_t>> page_access_counts_W(N_THR);
//unordered_map<uint64_t, uint64_t> page_sharers;
//unordered_map<uint64_t, uint64_t> page_Rs;
//unordered_map<uint64_t, uint64_t> page_Ws;
unordered_map<uint64_t, uint64_t> page_owner;
unordered_map<uint64_t, uint64_t> page_owner_CI;

uint64_t pages_in_pool = 0;

unordered_map<uint64_t, uint64_t> migration_per_page;
unordered_map<uint64_t, uint64_t> migration_per_page_CI;

//// for tracking access stats from past 1 billion insts
// phase->thread->(page_number, number of accesses) - TODO: Confirm this!
vector<vector<unordered_map<uint64_t, uint64_t>>> page_access_counts_history={};
vector<vector<unordered_map<uint64_t, uint64_t>>> page_access_counts_R_history={};
vector<vector<unordered_map<uint64_t, uint64_t>>> page_access_counts_W_history={};

// phase (presetly just length 1 because HISTORY_LEN=1) ->(page_number, number of accesses) - TODO: Confirm this!
vector<unordered_map<uint64_t, uint64_t>> page_Rs_history={};
vector<unordered_map<uint64_t, uint64_t>> page_Ws_history={};



//U64 curphase=0;
U64 phase_end_cycle=0;
bool any_trace_done=false;
std::ofstream misc_log_full("misc_log_full.txt");

uint64_t skipinsts=5000000000;
uint64_t phase_to_dump_pagemapping=0;
uint64_t num_phases_to_dump_pagemapping=20;

int process_phase(){
	cout<<"starting phase "<<curphase<<endl;
	misc_log_full<<"starting phase "<<curphase<<endl;
	phase_end_cycle=phase_end_cycle+PHASE_CYCLES;
	//page accesses
	//vector<unordered_map<uint64_t, uint64_t>> page_access_counts={};
	// sockets->(page_id, number_of_accesses)
	vector<unordered_map<uint64_t, uint64_t>> page_access_counts(N_SOCKETS);
	vector<unordered_map<uint64_t, uint64_t>> page_access_counts_R(N_SOCKETS);
	vector<unordered_map<uint64_t, uint64_t>> page_access_counts_W(N_SOCKETS);
	
	// sampled down version of page_access_counts: ANAND
	vector<unordered_map<uint64_t, uint64_t>> page_access_counts_sampled(N_SOCKETS);
	unordered_map<uint64_t, uint64_t> page_access_counts_sampled_joined;

	// threads->(page_id, number_of_accesses)
	vector<unordered_map<uint64_t, uint64_t>> page_access_counts_per_thread(N_THR);
	vector<unordered_map<uint64_t, uint64_t>> page_access_counts_R_per_thread(N_THR);
	vector<unordered_map<uint64_t, uint64_t>> page_access_counts_W_per_thread(N_THR);

	// sampled down version of page_access_counts_per_thread: ANAND
	vector<unordered_map<uint64_t, uint64_t>> page_access_counts_per_thread_sampled(N_THR);

	// ANAND - <page_id, <old_owner, new_owner, number_of_sharers>>
	unordered_map<uint64_t, array<uint64_t, 5>> migrated_pages_table;
	unordered_map<uint64_t, array<uint64_t, 5>> migrated_pages_table_CI;
	unordered_map<uint64_t, array<uint64_t, 5>> evicted_pages_table_CI;

	// (page_id, number of accesses)
	//list of all unique pages and their sharer count
	unordered_map<uint64_t, uint64_t> page_sharers={};
	//list of all unique pags and their R/W count
	unordered_map<uint64_t, uint64_t> page_Rs={};
	unordered_map<uint64_t, uint64_t> page_Ws={};

	std::multiset<std::pair<uint64_t, uint64_t>, migration_compare> sorted_candidates;

	// no output files generated from this
	// list of links (track traffic per link)
	U64 link_traffic_R[N_SOCKETS][N_SOCKETS]={0};
	U64 link_traffic_W[N_SOCKETS][N_SOCKETS]={0};
	U64 link_traffic_R_CI[N_SOCKETS][N_SOCKETS]={0};
	U64 link_traffic_W_CI[N_SOCKETS][N_SOCKETS]={0};
	U64	CI_traffic_R[N_SOCKETS]={0};
	U64	CI_traffic_W[N_SOCKETS]={0};

	// no output files generated from this
	// traffic at memory controller on each ndoe
	U64 mem_traffic[N_SOCKETS]={0};
	U64 mem_traffic_CI[N_SOCKETS]={0};
	
	U64 migrated_pages=0;
	U64 migrated_pages_CI=0;
	U64 pages_to_CI=0;


	//Histograms
	uint64_t hist_access_sharers[N_SOCKETS_OFFSET]={0};
	uint64_t hist_access_sharers_R[N_SOCKETS_OFFSET]={0};
	uint64_t hist_access_sharers_R_to_RWP[N_SOCKETS_OFFSET]={0};
	uint64_t hist_access_sharers_W[N_SOCKETS_OFFSET]={0};
	uint64_t hist_page_sharers[N_SOCKETS_OFFSET]={0};
	uint64_t hist_page_sharers_R[N_SOCKETS_OFFSET]={0};
	uint64_t hist_page_sharers_W[N_SOCKETS_OFFSET]={0};
	// TODO: Why is this 10 here?
	uint64_t hist_page_shareres_nacc[10][N_SOCKETS_OFFSET]={0};

	uint64_t hop_hist_W[N_SOCKETS_OFFSET][4]={0};
	uint64_t hop_hist_RtoRW[N_SOCKETS_OFFSET][4]={0};
	uint64_t hop_hist_RO[N_SOCKETS_OFFSET][4]={0};
	uint64_t hop_hist_W_CI[N_SOCKETS_OFFSET][4]={0};
	uint64_t hop_hist_RtoRW_CI[N_SOCKETS_OFFSET][4]={0};
	uint64_t hop_hist_RO_CI[N_SOCKETS_OFFSET][4]={0};

	U64 total_num_accs[N_THR]={0};

	//// @@@ TRACED READING for the phase
	/* Every access is read and recorded in page_access_counts 
	 * (per thread in the while loop, then consolidated to per-socket (4 threads per socket)
	 */
	#pragma omp parallel for
	for (int i=0; i<N_THR;i++){
		//U64 nompt=omp_get_num_threads();
		//cout<<"omp threads: "<<nompt<<endl;
		//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
		// Read Trace and get page access counts
		//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

		U64 tmp_numacc=0;

		unordered_map<uint64_t, uint64_t> pa_count;
		unordered_map<uint64_t, uint64_t> pa_count_R;
		unordered_map<uint64_t, uint64_t> pa_count_W;
		unordered_map<uint64_t, uint64_t> page_Rs_tmp;
		unordered_map<uint64_t, uint64_t> page_Ws_tmp;

		// create a sampled down version of above map: ANNAD
		unordered_map<uint64_t, uint64_t> pa_count_sampled;
		// unordered_map<uint64_t, uint64_t> pa_count_R_sampled;
		// unordered_map<uint64_t, uint64_t> pa_count_W_sampled;
		// unordered_map<uint64_t, uint64_t> page_Rs_tmp_sampled;
		// unordered_map<uint64_t, uint64_t> page_Ws_tmp_sampled;

		// create a map for instruction when page was updated: ANAND
		unordered_map<uint64_t, uint64_t> pa_count_sampled_last_ins;

		// use this counter for modulo with SAMPLING_PERIOD
		// uint64_t readline_counter = 0;

		char buffer[8];

		// read the first address
		uint64_t buf_val;
		size_t readsize = read_8B_line(&buf_val, buffer, trace[i]);
		// ANAND
		assert(readsize==8);

		// ANAND
		//read the first instruction
		uint64_t icount_val;
		readsize = read_8B_line(&icount_val, buffer, trace[i]);
		uint64_t first_icount_val = icount_val;

		while(readsize==8){

			if(buf_val==0xc0ffee){ // 1B inst phase done
				// ANAND: no need to read here again
				// read_8B_line(&buf_val, buffer, trace[i]);
				if(icount_val>=phase_end_cycle){
					//cout<<"inst count: "<<buf_val<<endl;
					break;
				}
				else{
					readsize = read_8B_line(&buf_val, buffer, trace[i]);
					assert(readsize==8);
					readsize = read_8B_line(&icount_val, buffer, trace[i]);
					assert(readsize==8);
					continue;
				}
			}
			tmp_numacc++;

			//parse page and RW
			U64 addr = buf_val;
			U64 page = addr >> PAGEBITS;
			U64 rwbit = addr & 1;
			bool isW = rwbit==1;

			//not doing anything with ins count in this script for now
			//just read and discard
			// ANAND
			//U64 icount_val=0;
			//read_8B_line(&icount_val, buffer, trace[i]);
			//cout<<"page: "<<page<<" icount: "<<icount_val<<endl;
			//if(icount_val>=phase_end_cycle){
				//TODO set flag to break while loop
				//let's just break... missing one access is fine
				//break;
			//}


			// add to page access count
			auto pa_it = pa_count.find(page);
			if(pa_it==pa_count.end()){ //didn't  find
				pa_count.insert({page,1});
				if(isW) {
					pa_count_W.insert({page,1});
					pa_count_R.insert({page,0});
				}
				else{
					pa_count_R.insert({page,1});
					pa_count_W.insert({page,0});
				}
			}
			else{
				pa_it->second=(pa_it->second)+1;
				if(isW) pa_count_W[page]=pa_count_W[page]+1;
				else pa_count_R[page]=pa_count_R[page]+1;
				//pa_count[page]=pa_count[page+1];
			}


			//increment R and Ws
			if(isW){
				//omp_set_lock(&page_W_lock);
				page_Ws_tmp[page]=page_Ws_tmp[page]+1;
				//omp_unset_lock(&page_W_lock);
			}
			else{
				//omp_set_lock(&page_R_lock);
				page_Rs_tmp[page]=page_Rs_tmp[page]+1;
				//omp_unset_lock(&page_R_lock);
			}

			// sample memory accesses with sampling period
			// pa_count_sampled, pa_count_R_sampled, pa_count_W_sampled
			// pa_count_sampled_last_ins

			if (!check_in_same_sampling_period(pa_count_sampled_last_ins[page], icount_val, first_icount_val)){
				pa_count_sampled[page] = pa_count_sampled[page] + 1;
				pa_count_sampled_last_ins[page] = icount_val;
				assert(pa_count_sampled[page] <= NBILLION/SAMPLING_PERIOD);
			}
			
			readsize = read_8B_line(&buf_val, buffer, trace[i]);
			assert(readsize==8);
			readsize = read_8B_line(&icount_val, buffer, trace[i]);
			assert(readsize==8);

		}

		if(readsize!=8){
			any_trace_done=true;
		}


		///////// @@@ coalescing local stats into one
		total_num_accs[i]+=tmp_numacc;
		#pragma omp critical
		{
			page_access_counts_per_thread[i]=pa_count;
			page_access_counts_per_thread_sampled[i]=pa_count_sampled;
			page_access_counts_W_per_thread[i]=(pa_count_W);
			page_access_counts_R_per_thread[i]=(pa_count_R);
			misc_log_full<<"t_"<<i<<" accesses this phase: "<<tmp_numacc<<endl;
			
		}
		for (const auto& pw : page_Ws_tmp) {
			U64 page = pw.first;
			U64 tmp_accs = pw.second;
			omp_set_lock(&page_W_lock);
			page_Ws[page]=page_Ws[page]+tmp_accs;
			omp_unset_lock(&page_W_lock);
		}
		for (const auto& pw : page_Rs_tmp) {
			U64 page = pw.first;
			U64 tmp_accs = pw.second;
			omp_set_lock(&page_R_lock);
			page_Rs[page]=page_Rs[page]+tmp_accs;
			omp_unset_lock(&page_R_lock);
		}

	}
	
	// @@@ consolidate page_access_counts per thread into per socket
	for(uint64_t ii=0; ii<N_THR; ii++){
		uint64_t socketid = ii>>2; //4 cores per socket
		for(const auto& pt : page_access_counts_per_thread[ii]){
			page_access_counts[socketid][pt.first]=page_access_counts[socketid][pt.first]+pt.second;
		}
		for(const auto& pt : page_access_counts_W_per_thread[ii]){
			page_access_counts_W[socketid][pt.first]=page_access_counts_W[socketid][pt.first]+pt.second;
		}
		for(const auto& pt : page_access_counts_R_per_thread[ii]){
			page_access_counts_R[socketid][pt.first]=page_access_counts_R[socketid][pt.first]+pt.second;
		}
	}

	// ANAND
	// No need to consolidate this over history as HISTORY_LEN = 1
	for(uint64_t ii=0; ii<N_THR; ii++){
		uint64_t socketid = ii>>2; //4 cores per socket
		for(const auto& pt : page_access_counts_per_thread_sampled[ii]){
			page_access_counts_sampled[socketid][pt.first]=page_access_counts_sampled[socketid][pt.first]+pt.second;
		}
	}

	/* @@@ following is legacy code from when making migration decsion for 100M instruction phase, based on profiling info from past 1B instructions
	 */
	//update access data from past 1 billion instructions
	page_access_counts_history.push_back(page_access_counts);
	page_access_counts_R_history.push_back(page_access_counts_R);
	page_access_counts_W_history.push_back(page_access_counts_W);
	page_Rs_history.push_back(page_Rs);
	page_Ws_history.push_back(page_Ws);
	if(page_access_counts_history.size()>HISTORY_LEN){ 
		//assume all the history vectors will have same length. as they should
		page_access_counts_history.erase(page_access_counts_history.begin());
		page_access_counts_R_history.erase(page_access_counts_R_history.begin());
		page_access_counts_W_history.erase(page_access_counts_W_history.begin());
		page_Rs_history.erase(page_Rs_history.begin());
		page_Ws_history.erase(page_Ws_history.begin());
	}

	//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
	// log sharers for each page for the past 1 Billion insts
	//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
	// build accesses per thread 
	vector<unordered_map<uint64_t, uint64_t>> page_access_counts_consol(N_SOCKETS);
	vector<unordered_map<uint64_t, uint64_t>> page_access_counts_R_consol(N_SOCKETS);
	vector<unordered_map<uint64_t, uint64_t>> page_access_counts_W_consol(N_SOCKETS);
	unordered_map<uint64_t, uint64_t> page_Rs_consol={};
	unordered_map<uint64_t, uint64_t> page_Ws_consol={};
	
	// ANAND
	unordered_map<uint64_t, uint64_t> page_access_counts_consol_joined;


	// sampled down version
	// vector<unordered_map<uint64_t, uint64_t>> page_access_counts_consol_sampled(N_SOCKETS);
	// this will be equal to page_access_counts_sampled(N_SOCKETS) because HISTORY_LEN is 1

	#pragma omp parallel for
	for(uint64_t ii=0;ii<N_SOCKETS;ii++){
		for(uint64_t jj=0;jj<page_access_counts_history.size();jj++){
			for (const auto& ppair : page_access_counts_history[jj][ii]){
				U64 page = ppair.first;
				page_access_counts_consol[ii][page]=page_access_counts_consol[ii][page]+ppair.second;
			}
			for (const auto& ppair : page_access_counts_R_history[jj][ii]){
				U64 page = ppair.first;
				page_access_counts_R_consol[ii][page]=page_access_counts_R_consol[ii][page]+ppair.second;
			}
			for (const auto& ppair : page_access_counts_W_history[jj][ii]){
				U64 page = ppair.first;
				page_access_counts_W_consol[ii][page]=page_access_counts_W_consol[ii][page]+ppair.second;
			}
		}
	}
	for(uint64_t jj=0;jj<page_Rs_history.size();jj++){
		for (const auto& ppair : page_Rs_history[jj]){
			U64 page = ppair.first;
			page_Rs_consol[page]=page_Rs_consol[page]+ppair.second;
		}
		for (const auto& ppair : page_Ws_history[jj]){
			U64 page = ppair.first;
			page_Ws_consol[page]=page_Ws_consol[page]+ppair.second;
		}
	}
	
	// calculate page sharers using page_access_counts_consol (consilated per socket)
	unordered_map<uint64_t, uint64_t> page_sharers_long={};
	for (const auto& pa_c : page_access_counts_consol) {
		for (const auto& ppair : pa_c) {
			U64 page = ppair.first;
			page_sharers_long[page]=page_sharers_long[page]+1;
			assert(page_sharers_long[page] < N_SOCKETS+1);
		}
	}




	//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
	// log sharers for each page for this phase
	//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
	// ANAND
	// for (const auto& pa_c : page_access_counts) {
	// No need to change here as even a single page access will be documented in the page_access_count_sampled
	// changed for consistency
	for (const auto& pa_c : page_access_counts_sampled) {
		for (const auto& ppair : pa_c) {
			U64 page = ppair.first;
			page_sharers[page]=page_sharers[page]+1;
			assert(page_sharers[page] < N_SOCKETS+1);
		}
	}
	

	// ANAND

	// page_access_counts_sampled_joined
	for (const auto& pa_c : page_access_counts_sampled) {
		for (const auto& ppair : pa_c) {
			U64 page = ppair.first;
			page_access_counts_sampled_joined[page]=page_access_counts_sampled_joined[page]+ppair.second;
		}
	}
	
	#if SORT_BY_SHARERS
		// sorted_candidates with just sharer info
		for (const auto& ppair : page_sharers_long){
			U64 page = ppair.first;
			U64 psharers = ppair.second;
			sorted_candidates.insert({page, psharers});
		}
	#else
		// sorted_candidates with acutal counts
		for (const auto& ppair : page_access_counts_sampled_joined){
			U64 page = ppair.first;
			U64 accs = ppair.second;
			sorted_candidates.insert({page, accs});
		}
		//cout<<"sorted candidates size: "<<sorted_candidates.size()<<endl;
	#endif

	// page_access_counts_consol_joined
	for (const auto& pa_c : page_access_counts_consol) {
		for (const auto& ppair : pa_c) {
			U64 page = ppair.first;
			page_access_counts_consol_joined[page]=page_access_counts_consol_joined[page]+ppair.second;
		}
	}


	//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
	// Populate access sharer histogram
	//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
	//dbg
	cout<<"hist_access_sharers[0]: " <<hist_access_sharers[0]<<endl;
	U64 pac_size = page_access_counts_consol.size();
	assert(pac_size==N_SOCKETS);
	for(U64 i=0; i<pac_size;i++){
		auto pac   = page_access_counts_consol[i];
		auto pac_W = page_access_counts_W_consol[i];
		auto pac_R = page_access_counts_R_consol[i];
		for (const auto& ppair : pac) {
			U64 page = ppair.first;
			U64 accs = ppair.second;
			U64 rds= pac_R[page];
			U64 wrs= pac_W[page];
			//U64 sharers = page_sharers[page];
			U64 sharers = page_sharers_long[page];
			assert(sharers>0);
			hist_access_sharers[sharers]=hist_access_sharers[sharers]+accs;
			hist_access_sharers_W[sharers]=hist_access_sharers_W[sharers]+wrs;
			if(page_Ws[page]!=0){
				hist_access_sharers_R_to_RWP[sharers]=hist_access_sharers_R_to_RWP[sharers]+rds;
			}
			else{
				hist_access_sharers_R[sharers]=hist_access_sharers_R[sharers]+rds;
			}
	
			//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
			// Populate hop histogram
			//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
			U64 owner=i;
			auto pp_it = page_owner.find(page);
			if(pp_it==page_owner.end()){ //new page first touch
				// ANAND: Below comment is old
				// this will favor thread 0. 
				// should be ok after the first phase..
				unordered_map<uint64_t, uint64_t> pa_count;
				vector<uint64_t> sharers = {};
				uint64_t jj = 0;
				// ANAND
				// for (const auto& pa_c : page_access_counts) {
				// Again no need to change here as we are not using the actual count, just the page id
				// changed for consistency
				for (const auto& pa_c : page_access_counts_sampled) {
					if (pa_c.find(page) != pa_c.end()) {
						sharers.push_back(jj);
					}
					jj++;
				}
				uint64_t ri = rand() % sharers.size();
				owner = sharers[ri];
				page_owner[page]=owner; 
				page_owner_CI[page]=owner; // also first encounter for CI
				
			}
			else{
				owner=pp_it->second;
			}
			U64 hop = gethop(i,owner);
			assert(hop<3);
			//U64 sharers = page_sharers[page];
			hop_hist_W[sharers][hop]=hop_hist_W[sharers][hop]+wrs;
			if(page_Ws[page]!=0){
				hop_hist_RtoRW[sharers][hop]=hop_hist_RtoRW[sharers][hop]+rds;
			}
			else{
				hop_hist_RO[sharers][hop]=hop_hist_RO[sharers][hop]+rds;
			}
			link_traffic_R[i][owner]=link_traffic_R[i][owner]+rds;
			link_traffic_W[i][owner]=link_traffic_W[i][owner]+wrs;
			mem_traffic[i]=mem_traffic[i]+rds+wrs;
			
			////@@@ repeat for CXL island @@@@@
			U64 owner_CI=i;
			auto pp_it_CI = page_owner_CI.find(page);
			if(pp_it_CI==page_owner_CI.end()){
				//shoudln't get here
				cout<<"code can't get here"<<endl;
				vector<uint64_t> sharers = {};
				uint64_t jj = 0;
				for (const auto& pa_c : page_access_counts) {
					if (pa_c.find(page) != pa_c.end()) {
						sharers.push_back(jj);
					}
					jj++;
				}
				uint64_t ri = rand() % sharers.size();
				owner_CI = sharers[ri];
				page_owner_CI[page]=owner_CI;
			}
			else{
				owner_CI=pp_it_CI->second;
			}
			U64 hop_CI = gethop(i,owner_CI);
			//U64 sharers_CI = page_sharers[page];
			hop_hist_W_CI[sharers][hop_CI]=hop_hist_W_CI[sharers][hop_CI]+wrs;
			if(page_Ws[page]!=0){
				hop_hist_RtoRW_CI[sharers][hop_CI]=hop_hist_RtoRW_CI[sharers][hop_CI]+rds;
			}
			else{
				hop_hist_RO_CI[sharers][hop_CI]=hop_hist_RO_CI[sharers][hop_CI]+rds;
			}
			if(owner_CI==CXO){
				CI_traffic_R[i]=CI_traffic_R[i]+rds;
				CI_traffic_W[i]=CI_traffic_W[i]+wrs;
				
			}
			else{
				link_traffic_R_CI[i][owner]=link_traffic_R_CI[i][owner]+rds;
				link_traffic_W_CI[i][owner]=link_traffic_W_CI[i][owner]+wrs;
				mem_traffic_CI[i]=mem_traffic_CI[i]+rds+wrs;
			}

		}
	}	




	//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
	// Populate page sharer histogram
	//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
	//for (const auto& pss : page_sharers) {
	for (const auto& pss : page_sharers_long) {
		U64 sharers = pss.second;
		U64 page = pss.first;
		hist_page_sharers[sharers]=hist_page_sharers[sharers]+1;
		if(page_Ws[page]!=0){
			hist_page_sharers_W[sharers]=hist_page_sharers_W[sharers]+1;
		}
		else{
			hist_page_sharers_R[sharers]=hist_page_sharers_R[sharers]+1;
		}
		U64 access_count = page_Rs[page] + page_Ws[page];
		U64 log_ac = static_cast<uint64_t>(round(log2(access_count)));
		if(log_ac>9) log_ac=9;
		hist_page_shareres_nacc[log_ac][sharers]=hist_page_shareres_nacc[log_ac][sharers]+1;
	}

	//save vpage to owner mapping, before reassigning for next phase
	// Record Stat Data from this phase
	string dir_path = generate_phasedirname();
	//if(mkdir(dir_name.c_str(),0777)==-1){
	//	perror(("Error creating directory " + dir_name).c_str());
    //} else {
    //    std::cout << "Created directory " << dir_name << std::endl;
    //}

    // Use the C++17 filesystem library to create directories
    // try {
    //     filesystem::create_directories(dir_path);
    //     std::cout << "Directories created successfully." << std::endl;
    // } catch (const std::exception& e) {
    //     std::cerr << "Error: " << e.what() << std::endl;
    //     return 1;
    // }

	std::string createDirectoryCmd = "mkdir -p " + dir_path;
	int result = std::system(createDirectoryCmd.c_str());
	cout << "result of creating repo " << result << endl;



	// @@@ Save page mapping that was used for this phase, before reassigning owners
	cout<<"dumping page to socket mapping"<<std::endl;
	save_uo_map(page_owner,"page_owner.txt\0");
	save_uo_map(page_owner_CI,"page_owner_CI.txt\0");


	//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
	// Reassign owners (Equivalent to "Algorithm 1")
	//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
	if(LIMIT_MIGRATION){
		uint64_t pingpong_lim = curphase / 4;
		uint64_t pool_cap = (page_owner_CI.size())/POOL_FRACTION;
		auto it_migration = sorted_candidates.begin();
		auto eviction_it = sorted_candidates.rbegin();

		uint64_t i=0;
		uint64_t i_cxi=0;
		bool eviction_candidate_was_hot=false;
		while((i<MIGRATION_LIMIT) || (i_cxi<MIGRATION_LIMIT)){
			auto reassign_1_page_start = std::chrono::high_resolution_clock::now();
			U64 page  = it_migration->first;
			U64 most_acc=0;
			U64 new_owner=INVAL_OWNER;
			U64 sharers = page_sharers_long[page];
			//assign random owner among sharers for pages with +8 sharers (if the page is not going to pool)
			if(sharers>=8){
				new_owner=rand()%N_SOCKETS;

				//cout<<"new owner: "<<new_owner<<endl;
			}
			else{
				for(U64 j=0; j<N_SOCKETS;j++){
					// ANAND
					//if(page_access_counts_consol[j][page]>most_acc){
					//	most_acc = page_access_counts[j][page];
					//	new_owner=j;
					//}

					// ANAND
					if(page_access_counts_sampled[j][page]>most_acc){
						most_acc = page_access_counts_sampled[j][page];
						new_owner=j;
					}
				}
			}


			if(i<MIGRATION_LIMIT){ // baseline
				//for baseline				
				U64 old_owner=0;
				old_owner=page_owner[page];
				if (migration_per_page[page] <= (pingpong_lim)) {
					if (old_owner != new_owner) {
						//if migrated. if no migration, don't increment
						i++;
						migrated_pages++;
						page_owner[page] = new_owner;
						migration_per_page[page] = migration_per_page[page] + 1;

						// ANAND
						migrated_pages_table[page] = {old_owner, new_owner, sharers, page_access_counts_consol_joined[page], page_access_counts_sampled_joined[page]};

					}
				}
			}
			auto t_baseline = std::chrono::high_resolution_clock::now();
			// std::chrono::duration<double> elapsed_baseline = t_baseline - t_new_owner;
			// std::cout << "Time taken for assigning baseline : " << elapsed_baseline.count() << " seconds\n";
			if(i_cxi<MIGRATION_LIMIT){ // cxl-island
				if (migration_per_page_CI[page] <= (pingpong_lim)) {
					U64 old_owner=page_owner_CI[page];
					if(!eviction_candidate_was_hot && (sharers >= SHARER_THRESHOLD)){
						if (old_owner != CXO) {
							pages_to_CI++;
							pages_in_pool++;
							i_cxi++;
							page_owner_CI[page] = CXO;
							migration_per_page_CI[page] = migration_per_page_CI[page] + 1;

							// ANAND
							migrated_pages_table_CI[page] = {old_owner, 100, sharers, page_access_counts_consol_joined[page], page_access_counts_sampled_joined[page]};

							//deal eviction if full capacity
							if(pool_cap <= pages_in_pool){
								//std::cout<<"DBG: is pool_cap being hit?"<<endl;
								//find eviction candidate
								uint64_t evicted_page=0;
								//auto it = sorted_candidates.rbegin();
								for(; eviction_it != sorted_candidates.rend(); ++eviction_it) {
									if(page_owner_CI[eviction_it->first] == CXO) {
										evicted_page = eviction_it->first;
										break;
									}
								}
								//std::cout<<"eviction_candidate access count: "<<eviction_it->second<<endl;
								if(eviction_it==sorted_candidates.rend()) {
									cout<<"WARNING: needed to find eviction candidate from pool but didn't find"<<endl;
									i_cxi=MIGRATION_LIMIT+1;
								}
								//U64 ev_most_acc=0;
								if((eviction_it->second)>=(it_migration->second)){
									//i_cxi=MIGRATION_LIMIT;
									cout<<"eviction candidate is hotter than new element into CXL island"<<endl;
									eviction_candidate_was_hot=true;
								}
								U64 ev_most_acc=0;
								U64 ev_new_owner=rand() % N_SOCKETS;								
								U64 ev_sharers = page_sharers_long[evicted_page];
								if(ev_sharers>=8){

								}
								else{
									// ANAND
									// for(U64 j=0; j<N_SOCKETS;j++){
									// 	if(page_access_counts_consol[j][evicted_page]>ev_most_acc){
									// 		ev_most_acc = page_access_counts[j][evicted_page];
									// 		ev_new_owner=j;
									// 	}
									// }

									for(U64 j=0; j<N_SOCKETS;j++){
										if(page_access_counts_sampled[j][evicted_page]>ev_most_acc){
											ev_most_acc = page_access_counts_sampled[j][evicted_page];
											ev_new_owner=j;
										}
									}
								}
								// screw this, takes too long. if u were on the island, u had a lot of sharers.

								page_owner_CI[evicted_page] = ev_new_owner;
								pages_in_pool--;
								i_cxi++;

								// ANAND
								evicted_pages_table_CI[evicted_page] = {100, ev_new_owner, ev_sharers, page_access_counts_consol_joined[page], page_access_counts_sampled_joined[page]};


							}
						}
						//}
					}
					else {
						if (old_owner != new_owner) {
							migrated_pages_CI++;
							page_owner_CI[page] = new_owner;
							i_cxi++;
							migration_per_page_CI[page] = migration_per_page_CI[page] + 1;

							// ANAND
							migrated_pages_table_CI[page] = {old_owner, new_owner, sharers, page_access_counts_consol_joined[page], page_access_counts_sampled_joined[page]};

						}
					}
				}
				
			}
			auto t_cxi = std::chrono::high_resolution_clock::now();
			std::chrono::duration<double> elapsed_cxi = t_cxi - t_baseline;
			//std::cout << "Time taken for assigning cxi : " << elapsed_cxi.count() << " seconds\n";

			it_migration++;
			if(it_migration==sorted_candidates.end()){//reached end of pages
				cout<<"reached end of sorted_candidates"<<endl;
				cout<<"sorted candidates size: "<<sorted_candidates.size()<<endl;
				break;
			}
			auto reassign_1_page_end = std::chrono::high_resolution_clock::now();
			std::chrono::duration<double> elapsed5 = reassign_1_page_end - reassign_1_page_start;
			//std::cout << "Time taken for reassigning page "<< i<< ": " << elapsed5.count() << " seconds\n";


			
		}

	}
	else{
		std::cout<<"ONLY SUPPORTS SIMULATION WITH MIGARTION LIMIT!"<<endl;
	}

	std::cout<<"reassign owners done"<<std::endl;

	U64 total_pages = page_sharers.size();
	U64 memory_touched = total_pages*PAGESIZE;
	U64 memory_touched_inMB = memory_touched>>20;
	U64 sumallacc=0;
	for(U64 i=0;i<N_SOCKETS;i++){
		sumallacc+=total_num_accs[i];
	}
	/// find all pages whose owner is CXI, to get memory size on CXI
	uint64_t CXI_count = std::count_if(page_owner_CI.begin(), page_owner_CI.end(), [](const std::pair<uint64_t, uint64_t>& pair) {
        return pair.second == CXO;
    });


	log_misc_stats(memory_touched_inMB,total_num_accs[0]
	,sumallacc,migrated_pages,migrated_pages_CI,pages_to_CI, CXI_count, misc_log_full);

	cout<<"baseline"<<endl;
	cout<<"memtraffic on node 5: "<<mem_traffic[5]<<endl;
	U64 linktraffic_5_12=link_traffic_R[5][12]+link_traffic_R[12][5]+link_traffic_W[5][12]+link_traffic_W[12][5];
	cout<<"link traffic between 5 and 12: "<< linktraffic_5_12<<endl;
	
	cout<<"CXL Island"<<endl;
	cout<<"memtraffic on node 5: "<<mem_traffic_CI[5]<<endl;
	U64 linktraffic_5_12_CI=link_traffic_R_CI[5][12]+link_traffic_R_CI[12][5]+link_traffic_W_CI[5][12]+link_traffic_W_CI[12][5];
	cout<<"link traffic between 5 and 12: "<< linktraffic_5_12_CI<<endl;
	cout<<"traffic to CI from a single node(5): "<<CI_traffic_R[5]+CI_traffic_W[5]<<endl;

	auto sorted_candidates_it = sorted_candidates.begin();
	cout<<"Sampled access top 1 " << sorted_candidates_it->first << " " << sorted_candidates_it->second << endl;
	sorted_candidates_it++;
	cout<<"Sampled access top 2 " << sorted_candidates_it->first << " " << sorted_candidates_it->second << endl;
	sorted_candidates_it++;
	cout<<"Sampled access top 3 " << sorted_candidates_it->first << " " << sorted_candidates_it->second << endl;
	sorted_candidates_it++;
	cout<<"Sampled access top 4 " << sorted_candidates_it->first << " " << sorted_candidates_it->second << endl;
	sorted_candidates_it++;
	cout<<"Sampled access top 5 " << sorted_candidates_it->first << " " << sorted_candidates_it->second << endl;


	if(sorted_candidates.size()>MIGRATION_LIMIT){
		auto sorted_candidates_it2 = sorted_candidates.begin();
		for(uint64_t i =0; i<MIGRATION_LIMIT;i++){
			sorted_candidates_it2++;
		}
		std::cout<<"count @ "<<MIGRATION_LIMIT<<"th entry: "<<sorted_candidates_it2->second<<std::endl;
	}

	// @@@@@ saving stat tracking files
	savearray(hist_access_sharers, N_SOCKETS_OFFSET,"access_hist.txt\0");
	savearray(hist_access_sharers_W, N_SOCKETS_OFFSET,"access_hist_W.txt\0");
	savearray(hist_access_sharers_R_to_RWP, N_SOCKETS_OFFSET,"access_hist_R_to_RWP.txt\0");
	savearray(hist_access_sharers_R, N_SOCKETS_OFFSET,"access_hist_R.txt\0");
	savearray(hist_page_sharers, N_SOCKETS_OFFSET,"page_hist.txt\0");
	savearray(hist_page_sharers_W, N_SOCKETS_OFFSET,"page_hist_W.txt\0");
	savearray(hist_page_sharers_R, N_SOCKETS_OFFSET,"page_hist_R.txt\0");
	save2Darr(hist_page_shareres_nacc, N_SOCKETS_OFFSET, "page_hist_nacc.txt\0");

	save_hophist(hop_hist_W,N_SOCKETS_OFFSET, "hop_hist_W.txt\0");
	save_hophist(hop_hist_RO,N_SOCKETS_OFFSET, "hop_hist_RO.txt\0");
	save_hophist(hop_hist_RtoRW,N_SOCKETS_OFFSET, "hop_hist_RtoRW.txt\0");

	save_hophist(hop_hist_W_CI,N_SOCKETS_OFFSET, "hop_hist_W_CI.txt\0");
	save_hophist(hop_hist_RO_CI,N_SOCKETS_OFFSET, "hop_hist_RO_CI.txt\0");
	save_hophist(hop_hist_RtoRW_CI,N_SOCKETS_OFFSET, "hop_hist_RtoRW_CI.txt\0");

	// ANAND
	save_uo_array_map(migrated_pages_table,"migrated_pages_table.txt\0");
	save_uo_array_map(migrated_pages_table_CI,"migrated_pages_table_CI.txt\0");
	save_uo_array_map(evicted_pages_table_CI,"evicted_pages_table_CI.txt\0");
	
	curphase=curphase+1;
	return 0;
}

int main(){

	omp_init_lock(&page_W_lock);
	omp_init_lock(&page_R_lock);
	//omp_init_lock(&page_owner_lock);
	//omp_init_lock(&page_owner_CI_lock);
	//omp_init_lock(&hop_hist_lock);
	//omp_init_lock(&hop_hist_CI_lock);
	
	assert(HISTORY_LEN >=1);
	
	curphase=0;
	
	phase_to_dump_pagemapping=(skipinsts/PHASE_CYCLES); 
	//for initial check
	//phase_to_dump_pagemapping=1;
	assert(phase_to_dump_pagemapping>=0);
	cout<<"phase to dump pagemapping "<<phase_to_dump_pagemapping<<std::endl;
	//-1 because mapping is deteremined at the end of previus phase

	for(int i=0; i<N_THR;i++){
		std::ostringstream tfname;
		tfname << "memtrace_t" << (i+THR_OFFSET_VAL) << ".out";
		if(i==N_THR-1){
		std::cout<<tfname.str()<<std::endl;
		}
    	trace[i] = fopen(tfname.str().c_str(), "rb");
	}

	process_phase(); //do a single phase for warmup(page allocation)
	//while(!any_trace_done){
	for(int i=0;i<1000;i++){ //putting a bound for now
		if(any_trace_done) break;
		process_phase();
	}
	cout<<"processed "<<curphase<<" phases"<<endl;
	for(int i=0; i<N_THR;i++){
		fclose(trace[i]);
	}
	return 0;

}

