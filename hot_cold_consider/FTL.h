// 덮어쓸 파일: FTL.h

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
const int HOT_LPN_THRESHOLD = 10;

class FTL {
public:
    FTL();
    bool write(int lpn);
    void read(int lpn);
    double getWAF() const;
    void print_debug_state();

    // ✅ --- [기존 Getter 함수들] ---
    int get_hot_active_block() const;
    int get_cold_active_block() const;
    void get_block_hot_cold_counts(int block_idx, int& hot_count, int& cold_count) const;
    const NandFlash& get_nand_flash() const;
    // --------------------------------

    // ✅ --- [추가] LPN 쓰기 횟수 맵을 가져오는 Getter ---
    const std::map<int, int>& get_lpn_write_counts() const;
    // -------------------------------------------------

private:
    NandFlash nand_;
    std::map<int, PPA> l2p_mapping_;

    int hot_active_block_;
    int cold_active_block_;

    long long user_writes_;
    long long user_reads_;

    std::map<int, int> lpn_write_counts_;

    bool garbage_collect();
    int find_victim_block_greedy();
    int get_free_block();
    void wear_leveling();

    bool get_new_page(PPA& ppa, int lpn);

    int count_free_blocks();
};

#endif // FTL_H