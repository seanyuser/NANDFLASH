#ifndef FTL_GREEDY_H
#define FTL_GREEDY_H

#include "NandFlash.h"
#include <vector>
#include <map>

// (규격은 FTL.h와 동일)
const int NUM_LOGICAL_PAGES = NUM_BLOCKS * PAGES_PER_BLOCK * 0.75;

struct PPA {
    int block;
    int page;
};

const int GC_THRESHOLD = 5;

// ✅ "Greedy FTL" (단순 FTL) 클래스
class FTL_Greedy {
public:
    FTL_Greedy();
    bool write(int lpn);
    void read(int lpn);
    double getWAF() const;
    void print_debug_state(); // (단순화된 디버그 함수)

private:
    NandFlash nand_;
    std::map<int, PPA> l2p_mapping_;
    
    // ✅ Active Block이 Hot/Cold 구분 없이 단 하나
    int active_block_;

    long long user_writes_;
    long long user_reads_;

    // ✅ "학습"에 필요한 lpn_write_counts_ 맵 없음

    bool garbage_collect();
    int find_victim_block_greedy();
    int get_free_block();
    
    // ✅ "온도" 판단이 필요 없는 단순한 get_new_page
    bool get_new_page(PPA& ppa); 
    
    int count_free_blocks();
};

#endif // FTL_GREEDY_H