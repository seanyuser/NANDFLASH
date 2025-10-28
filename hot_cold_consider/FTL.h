#ifndef FTL_H
#define FTL_H

#include "NandFlash.h"
#include <vector>
#include <map>

const int NUM_LOGICAL_PAGES = NUM_BLOCKS * PAGES_PER_BLOCK * 0.75;

struct PPA {
    int block;
    int page;
};

const int GC_THRESHOLD = 5;

// ✅ LPN이 몇 번 이상 덮어쓰기되면 'Hot'으로 간주할지에 대한 임계값
const int HOT_LPN_THRESHOLD = 10; 

class FTL {
public:
    FTL();
    bool write(int lpn);
    void read(int lpn);
    double getWAF() const;
    void print_debug_state();

private:
    NandFlash nand_;
    std::map<int, PPA> l2p_mapping_;
    
    // ✅ Active Block을 Hot/Cold 두 개로 분리
    int hot_active_block_;  // Hot 데이터만 쓰는 블록
    int cold_active_block_; // Cold 데이터만 쓰는 블록

    long long user_writes_;
    long long user_reads_;

    // ✅ LPN별 쓰기 횟수를 "학습"하기 위한 맵
    std::map<int, int> lpn_write_counts_;

    bool garbage_collect();
    int find_victim_block_greedy();
    int get_free_block();
    void wear_leveling();
    
    // ✅ LPN을 인자로 받아 "온도"를 판단하도록 변경
    bool get_new_page(PPA& ppa, int lpn); 
    
    int count_free_blocks();
};

#endif // FTL_H
