// 덮어쓸 파일: FTL.h

#ifndef FTL_H
#define FTL_H

#include "NandFlash.h"
#include <vector>
#include <map>
#include <list> // ✅ 리스트 관리를 위해 <list> 또는 <vector> 추가 (vector 사용)

extern int gc_victim_strategy;

const int NUM_LOGICAL_PAGES = NUM_BLOCKS * PAGES_PER_BLOCK * 0.75;

struct PPA {
    int block;
    int page;
};

const int GC_THRESHOLD = 5;
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
    
    int hot_active_block_;  
    int cold_active_block_; 

    long long user_writes_;
    long long user_reads_;

    std::map<int, int> lpn_write_counts_;

    // ✅ --- [추가] 사용자님이 제안한 "레이블" (블록 리스트) ---
    // (vector 대신 list를 사용하면 중간 삭제가 더 효율적이지만,
    //  여기서는 vector를 사용하고 탐색/삭제 로직을 구현합니다.)
    std::vector<int> closed_hot_blocks_;  // Hot 데이터로 꽉 찬 블록 리스트
    std::vector<int> closed_cold_blocks_; // Cold 데이터로 꽉 찬 블록 리스트
    // --------------------------------------------------------

    bool garbage_collect();
    
    // ✅ [변경] "Greedy" 대신 "Smart"한 탐색 함수로 변경
    int find_victim_block_smart(); 

    int get_free_block();
    void wear_leveling();
    
    bool get_new_page(PPA& ppa, int lpn); 
    
    int count_free_blocks();
};

#endif // FTL_H