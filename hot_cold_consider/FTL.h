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
const int HOT_LPN_THRESHOLD = 1;

class FTL {
public:
    FTL();
    bool write(int lpn);
    void read(int lpn);
    double getWAF() const;
    void print_debug_state();

    // ✅ --- [추가] main에서 상태 관찰을 위한 Getter 함수 ---
    int get_hot_active_block() const;
    int get_cold_active_block() const;
    // 특정 블록의 Hot/Cold 페이지 수를 계산하는 함수 (NandFlash 객체 필요)
    void get_block_hot_cold_counts(int block_idx, int& hot_count, int& cold_count) const;
    // NandFlash 객체에 직접 접근하기 위한 const 참조 반환
    const NandFlash& get_nand_flash() const;
    // --------------------------------------------------------

private:
    NandFlash nand_;
    std::map<int, PPA> l2p_mapping_;

    int hot_active_block_;
    int cold_active_block_;

    long long user_writes_;
    long long user_reads_;

    std::map<int, int> lpn_write_counts_;

    bool garbage_collect();
    int find_victim_block_greedy(); // 이름은 greedy지만 실제론 Smart 로직 사용 가능
    int get_free_block();
    void wear_leveling();

    bool get_new_page(PPA& ppa, int lpn);

    int count_free_blocks();
};

#endif // FTL_H