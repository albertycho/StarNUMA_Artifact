#ifndef __MTTEST_HH
#define __MTTEST_HH

#include "kvrandom.hh"
#include "str.hh"

const int mycsbaAggrFactor = 256;
const uint32_t mycsbaAggrMask = mycsbaAggrFactor - 1;
const long mycsbaSeed = 3242323423L;
//const int mycsbaDbSize = 1000000;
//const int mycsbaDbSize = 2000000;
//const int mycsbaDbSize = 131072; // 128K entries
//const int mycsbaDbSize = 5000000; // 5M
//const int mycsbaDbSize = 10000000; // 10GB
const int mycsbaDbSize = 12000000; // ~35GB with 256B keys
//const int mycsbaDbSize = 100000000; // 100GB
//const int mycsbaDbSize = 500000000; // 500GB
//const int mycsbaKeySize = 4 + 18 + 1;
const int mycsbaKeySize = 18 + 1;
//const int mycsbaValSize = 12; // int32_t can be up to 2B => 10 digits + minus sign
//const int mycsbaValSize = 36; // int32_t can be up to 2B => 10 digits + minus sign
//const int mycsbaValSize = 512; // int32_t can be up to 2B => 10 digits + minus sign
const int mycsbaValSize = 256; // int32_t can be up to 2B => 10 digits + minus sign
//const int mycsbaValSize = 128; // int32_t can be up to 2B => 10 digits + minus sign

enum ReqType { GET, PUT };
enum Status { SUCCESS, FAILURE };

struct Request {
    ReqType type;
    char key[mycsbaKeySize];
    char val[mycsbaValSize];
};

struct Response {
    Status status;
};

static void genKeyVal(kvrandom_lcg_nr& rand, char* key, char* val) {
    //strcpy(key, "user");
    //int p = 4;
	int p = 0;
    for (int i = 0; i < 18; ++i, ++p) {
        key[p] = '0' + (rand.next() % 10);
    }

    key[p] = 0;

    //int32_t value = static_cast<int32_t>(rand.next());
    //sprintf(val, "%d", value);
	for(int i=0;i<mycsbaValSize;i++){
		val[i]='0'+(rand.next()%10);
	}
	val[mycsbaValSize-1]=0;

    return;
}

#endif
