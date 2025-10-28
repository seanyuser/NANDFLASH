#include "FTL.h"
#include <iostream>
#include <iomanip> 
#include <algorithm>

FTL::FTL() : user_writes_(0), user_reads_(0) {
    for (int i = 0; i < NUM_BLOCKS; ++i) {
        nand_.erase(i);
    }
    // ✅ Hot/Cold Active Block을 서로 다른 블록으로 초기화
    hot_active_block_ = 0; 
    cold_active_block_ = 1;
}

bool FTL::write(int lpn) {
    user_writes_++;
    
    // ✅ LPN 쓰기 횟수를 "학습" (1 증가)
    lpn_write_counts_[lpn]++;

    while (count_free_blocks() < GC_THRESHOLD) {
        if (!garbage_collect()) {
            std::cerr << "Write failed because garbage_collect failed during pre-check." << std::endl;
            print_debug_state();
            return false;
        }
    }

    if (l2p_mapping_.count(lpn)) {
        PPA old_ppa = l2p_mapping_[lpn];
        nand_.blocks[old_ppa.block].pages[old_ppa.page].state = PageState::INVALID;
        nand_.blocks[old_ppa.block].valid_pages--;
        nand_.blocks[old_ppa.block].invalid_pages++;
    }

    PPA new_ppa;
    // ✅ get_new_page에 lpn을 전달하여 "온도"를 판단하게 함
    if (!get_new_page(new_ppa, lpn)) {
        std::cerr << "Write failed because get_new_page failed." << std::endl;
        print_debug_state();
        return false;
    }
    
    nand_.write(new_ppa.block, new_ppa.page, lpn);
    l2p_mapping_[lpn] = new_ppa;

    return true;
}

void FTL::read(int lpn) {
    user_reads_++;
    if (l2p_mapping_.count(lpn)) {
        PPA ppa = l2p_mapping_[lpn];
        nand_.read(ppa.block, ppa.page);
    }
}

// ✅ "온도"를 판단하여 Hot/Cold 블록에 페이지를 할당하는 핵심 함수
bool FTL::get_new_page(PPA& ppa, int lpn) {
    // LPN의 쓰기 횟수를 확인하여 "온도" 판단
    bool is_hot = (lpn_write_counts_.count(lpn) && lpn_write_counts_[lpn] > HOT_LPN_THRESHOLD);

    if (is_hot) {
        // --- Hot 데이터 경로 ---
        if (nand_.blocks[hot_active_block_].current_page >= PAGES_PER_BLOCK) {
            hot_active_block_ = get_free_block(); // Hot 블록이 꽉 차면 새 블록 할당
            if (hot_active_block_ == -1) {
                std::cerr << "Fatal Error in get_new_page: No free block for HOT writes." << std::endl;
                return false;
            }
        }
        ppa = {hot_active_block_, nand_.blocks[hot_active_block_].current_page};
    } else {
        // --- Cold 데이터 경로 ---
        if (nand_.blocks[cold_active_block_].current_page >= PAGES_PER_BLOCK) {
            cold_active_block_ = get_free_block(); // Cold 블록이 꽉 차면 새 블록 할당
            if (cold_active_block_ == -1) {
                std::cerr << "Fatal Error in get_new_page: No free block for COLD writes." << std::endl;
                return false;
            }
        }
        ppa = {cold_active_block_, nand_.blocks[cold_active_block_].current_page};
    }
    return true;
}

int FTL::count_free_blocks() {
    int count = 0;
    for (int i = 0; i < NUM_BLOCKS; i++) {
        // ✅ Hot/Cold Active 블록은 예비 블록이 아님
        if (i == hot_active_block_ || i == cold_active_block_) continue;
        if (nand_.blocks[i].current_page == 0) {
            count++;
        }
    }
    return count;
}


// ✅ "Hot/Cold 분리 GC" 로직 (블록 낭비 버그 수정됨)
bool FTL::garbage_collect() {
    int victim_idx = find_victim_block_greedy();
    if (victim_idx == -1) {
        return true; 
    }

    Block& victim_block = nand_.blocks[victim_idx];

    // 1. 희생양 블록을 스캔하여 복사할 Hot/Cold 페이지 수 계산
    int hot_pages_to_copy = 0;
    int cold_pages_to_copy = 0;
    for (int i = 0; i < PAGES_PER_BLOCK; ++i) {
        if (victim_block.pages[i].state == PageState::VALID) {
            int lpn = victim_block.pages[i].logical_page_number;
            // ✅ 이 논리는 항상 false이지만, (이전 논의처럼) 연산 오버헤드 외에 해가 없음
            if (lpn_write_counts_.count(lpn) && lpn_write_counts_[lpn] > HOT_LPN_THRESHOLD) {
                hot_pages_to_copy++;
            } else {
                cold_pages_to_copy++;
            }
        }
    }

    // 💡 [수정됨] 디버그 출력을 원하면 이 줄의 주석을 해제하세요.
    // std::cout << "[GC] Victim: " << std::setw(3) << victim_idx 
    //           << " | HotPagesToCopy: " << std::setw(2) << hot_pages_to_copy 
    //           << " | ColdPagesToCopy: " << std::setw(2) << cold_pages_to_copy 
    //           << std::endl;


    // 2. "스마트 병합" 시도: Active 블록에 공간이 있는지 확인
    Block& hot_active = nand_.blocks[hot_active_block_];
    Block& cold_active = nand_.blocks[cold_active_block_];
    bool can_merge_hot = (PAGES_PER_BLOCK - hot_active.current_page) >= hot_pages_to_copy;
    bool can_merge_cold = (PAGES_PER_BLOCK - cold_active.current_page) >= cold_pages_to_copy;

    if (can_merge_hot && can_merge_cold) {
        // --- 전략 1: "스마트 병합" 성공 ---
        for (int i = 0; i < PAGES_PER_BLOCK; ++i) {
            if (victim_block.pages[i].state == PageState::VALID) {
                int lpn = victim_block.pages[i].logical_page_number;
                // ✅ 이 로직도 항상 false이지만, 해가 없음
                bool is_hot = (lpn_write_counts_.count(lpn) && lpn_write_counts_[lpn] > HOT_LPN_THRESHOLD);
                
                PPA new_ppa;
                if (is_hot) {
                    new_ppa = {hot_active_block_, hot_active.current_page};
                } else {
                    new_ppa = {cold_active_block_, cold_active.current_page};
                }
                nand_.write(new_ppa.block, new_ppa.page, lpn);
                l2p_mapping_[lpn] = new_ppa;
            }
        }
        nand_.erase(victim_idx);
        return true;
    }

    // --- 전략 2: "스마트 복사" (병합 실패 시) ---
    
    // ✅ [수정됨] 필요한 블록만 할당하도록 로직 변경
    int new_hot_block = -1;
    int new_cold_block = -1;
    int original_hot_active = hot_active_block_;
    int original_cold_active = cold_active_block_;

    if (hot_pages_to_copy > 0) {
        new_hot_block = get_free_block();
        if (new_hot_block == -1) {
             std::cerr << "GC Fatal Error: Not enough free blocks for Hot copy!" << std::endl;
             return false;
        }
        // 임시 설정 (get_free_block 중복 할당 방지)
        hot_active_block_ = new_hot_block; 
    }

    if (cold_pages_to_copy > 0) {
        new_cold_block = get_free_block();
        // 임시 설정 복구
        hot_active_block_ = original_hot_active; 
        if (new_cold_block == -1) {
             std::cerr << "GC Fatal Error: Not enough free blocks for Cold copy!" << std::endl;
             return false; 
        }
        // 임시 설정 (get_free_block 중복 할당 방지)
        cold_active_block_ = new_cold_block;
    }
    
    // 임시 설정 모두 원래대로 복구
    hot_active_block_ = original_hot_active;
    cold_active_block_ = original_cold_active;


    // --- (이하 for 루프는 기존과 동일) ---
    for (int i = 0; i < PAGES_PER_BLOCK; ++i) {
        if (victim_block.pages[i].state == PageState::VALID) {
            int lpn = victim_block.pages[i].logical_page_number;
            // ✅ 이 로직도 항상 false이지만, 해가 없음
            bool is_hot = (lpn_write_counts_.count(lpn) && lpn_write_counts_[lpn] > HOT_LPN_THRESHOLD);
            
            PPA new_ppa;
            if (is_hot) {
                // 이 경로는 hot_pages_to_copy > 0 일 때만 실행됨
                new_ppa = {new_hot_block, nand_.blocks[new_hot_block].current_page};
            } else {
                // 이 경로는 cold_pages_to_copy > 0 일 때만 실행됨
                new_ppa = {new_cold_block, nand_.blocks[new_cold_block].current_page};
            }
            
            nand_.write(new_ppa.block, new_ppa.page, lpn);
            l2p_mapping_[lpn] = new_ppa;
        }
    }
    
    nand_.erase(victim_idx);

    // ✅ [수정됨] 새 블록이 할당된 경우에만 Active Block을 교체
    if (new_hot_block != -1) {
        hot_active_block_ = new_hot_block;
    }
    if (new_cold_block != -1) {
        cold_active_block_ = new_cold_block;
    }
    
    return true;
}


int FTL::find_victim_block_greedy() {
    int victim_block = -1;
    int max_invalid_pages = -1;

    for (int i = 0; i < NUM_BLOCKS; ++i) {
        // ✅ Hot/Cold Active 블록 2개 모두 GC 대상에서 제외
        if (i == hot_active_block_ || i == cold_active_block_) continue; 
        
        if (nand_.blocks[i].invalid_pages > max_invalid_pages) {
            max_invalid_pages = nand_.blocks[i].invalid_pages;
            victim_block = i;
        }
    }

    if (max_invalid_pages > 0) {
        return victim_block;
    }

    int min_valid_pages = PAGES_PER_BLOCK + 1;
    int fallback_victim = -1;
    for (int i = 0; i < NUM_BLOCKS; ++i) {
        // ✅ Hot/Cold Active 블록 2개 모두 GC 대상에서 제외
        if (i == hot_active_block_ || i == cold_active_block_) continue; 
        if (nand_.blocks[i].current_page == 0) continue;
        if (nand_.blocks[i].valid_pages < min_valid_pages) {
            min_valid_pages = nand_.blocks[i].valid_pages;
            fallback_victim = i;
        }
    }
    return fallback_victim;
}

int FTL::get_free_block() {
    for (int i = 0; i < NUM_BLOCKS; i++) {
        // ✅ Hot/Cold Active 블록은 Free가 아님
        if (i == hot_active_block_ || i == cold_active_block_) continue;
        if (nand_.blocks[i].current_page == 0) {
            return i;
        }
    }
    return -1;
}

// (wear_leveling과 getWAF, print_debug_state 함수는 기존과 동일하게 유지)

void FTL::wear_leveling() {
    // ... (기존 코드와 동일, 단 active_block_ 대신 hot/cold 중 하나를 선택해야 함)
    // (이 로직은 Hot/Cold 분리 시 더 복잡해지므로, 일단은 hot_active_block_ 기준으로 둠)
    
    int min_erase_count = nand_.blocks[0].erase_count;
    int min_erase_idx = 0;

    for (int i = 1; i < NUM_BLOCKS; ++i) {
        if (nand_.blocks[i].erase_count < min_erase_count) {
            min_erase_count = nand_.blocks[i].erase_count;
            min_erase_idx = i;
        }
    }

    const int WEAR_LEVELING_THRESHOLD = 5;
    // ✅ 일단 hot_active_block_을 기준으로 마모도 비교
    if (nand_.blocks[hot_active_block_].erase_count > min_erase_count + WEAR_LEVELING_THRESHOLD) {
        int free_block = get_free_block();
        if (free_block != -1) {
            Block& min_worn_block = nand_.blocks[min_erase_idx];
             for (int i = 0; i < PAGES_PER_BLOCK; ++i) {
                if (min_worn_block.pages[i].state == PageState::VALID) {
                    int lpn = min_worn_block.pages[i].logical_page_number;
                    // ✅ (수정 필요) 이 데이터가 Hot인지 Cold인지 알 수 없으므로,
                    // 일단 free_block에 쓴다. (이로 인해 오염 발생 가능)
                    PPA new_ppa = {free_block, nand_.blocks[free_block].current_page};
                    nand_.write(new_ppa.block, new_ppa.page, lpn);
                    l2p_mapping_[lpn] = new_ppa;
                }
            }
            nand_.erase(min_erase_idx);
        }
    }
}

double FTL::getWAF() const {
    if (user_writes_ == 0) {
        return 0.0;
    }
    return static_cast<double>(nand_.get_nand_writes()) / user_writes_;
}

void FTL::print_debug_state() {
    std::cout << "\n--- NAND FLASH DEBUG STATE ---" << std::endl;
    // ✅ Hot/Cold Active Block 정보 출력
    std::cout << "Hot Active Block: " << hot_active_block_ << std::endl;
    std::cout << "Cold Active Block: " << cold_active_block_ << std::endl;
    std::cout << "Free Blocks Count: " << count_free_blocks() << std::endl;
    std::cout << std::left << std::setw(8) << "Block"
              << std::setw(8) << "Valid"
              << std::setw(10) << "Invalid"
              << std::setw(8) << "Current"
              << std::setw(8) << "Erase" << std::endl;
    std::cout << "-----------------------------------------------" << std::endl;
    for (int i = 0; i < NUM_BLOCKS; ++i) {
        if (nand_.blocks[i].valid_pages > 0 || nand_.blocks[i].invalid_pages > 0 || nand_.blocks[i].current_page > 0 || i == hot_active_block_ || i == cold_active_block_) {
             std::cout << std::left << std::setw(8) << i
                       << std::setw(8) << nand_.blocks[i].valid_pages
                       << std::setw(10) << nand_.blocks[i].invalid_pages
                       << std::setw(8) << nand_.blocks[i].current_page
                       << std::setw(8) << nand_.blocks[i].erase_count << std::endl;
        }
    }
    std::cout << "-----------------------------------------------\n" << std::endl;
}
