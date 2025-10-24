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

const int GC_THRESHOLD = 5; // 예비 블록을 조금 더 넉넉하게 5개로 설정

class FTL {
public:
    FTL();
    bool write(int lpn); // ✅ 반환 타입을 bool로 변경
    void read(int lpn);
    double getWAF() const;
    void print_debug_state(); // ✅ 디버그 정보 출력 함수 선언

private:
    NandFlash nand_;
    std::map<int, PPA> l2p_mapping_;
    int active_block_;
    long long user_writes_;
    long long user_reads_;

    bool garbage_collect(); // ✅ 반환 타입을 bool로 변경
    int find_victim_block_greedy();
    int get_free_block();
    void wear_leveling();
    
    // ✅ PPA를 인자로 받아 성공 여부를 반환하도록 변경
    bool get_new_page(PPA& ppa); 
    
    int count_free_blocks();
};

#endif // FTL_H
