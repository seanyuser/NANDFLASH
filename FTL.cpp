#include "FTL.h"
#include <iostream>
#include <iomanip> // for std::setw
#include <algorithm>

FTL::FTL() : user_writes_(0), user_reads_(0) {
    for (int i = 0; i < NUM_BLOCKS; ++i) {
        nand_.erase(i);
    }
    active_block_ = 0;
}

bool FTL::write(int lpn) {
    user_writes_++;

    // 1. GC가 필요한지 먼저 확인하고, 필요하다면 예비 공간을 충분히 확보한다.
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

    // 2. 공간이 확보된 것을 확인한 후, 안전하게 새 페이지를 할당받는다.
    PPA new_ppa;
    if (!get_new_page(new_ppa)) {
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

bool FTL::get_new_page(PPA& ppa) {
    if (nand_.blocks[active_block_].current_page >= PAGES_PER_BLOCK) {
        active_block_ = get_free_block();
        if (active_block_ == -1) {
            std::cerr << "Fatal Error in get_new_page: No free block available for new writes." << std::endl;
            return false;
        }
    }
    ppa = {active_block_, nand_.blocks[active_block_].current_page};
    return true;
}

int FTL::count_free_blocks() {
    int count = 0;
    for (int i = 0; i < NUM_BLOCKS; i++) {
        if (nand_.blocks[i].current_page == 0) {
            count++;
        }
    }
    return count;
}


// ✅ 최종 수정: "병합"을 우선하는 스마트 GC 로직
bool FTL::garbage_collect() {
    // 1. 희생양 블록을 찾는다.
    int victim_idx = find_victim_block_greedy();
    if (victim_idx == -1) {
        return true; // GC 할 대상이 없음 (오류 아님)
    }

    Block& victim_block = nand_.blocks[victim_idx];
    int valid_page_count = victim_block.valid_pages;

    // --- 전략 1: 현재 Active Block과 병합 시도 (순이익 +1 예비 블록) ---
    Block& active = nand_.blocks[active_block_];
    if ((PAGES_PER_BLOCK - active.current_page) >= valid_page_count) {
        for (int i = 0; i < PAGES_PER_BLOCK; ++i) {
            if (victim_block.pages[i].state == PageState::VALID) {
                int lpn = victim_block.pages[i].logical_page_number;
                PPA new_ppa = {active_block_, active.current_page};
                nand_.write(new_ppa.block, new_ppa.page, lpn);
                l2p_mapping_[lpn] = new_ppa;
            }
        }
        nand_.erase(victim_idx); // 희생양을 지워 예비 블록으로 만듦
        return true;
    }

    // --- 전략 2: 병합 실패 시, 기존 방식으로 새 블록에 복사 (순이익 0) ---
    int new_block_idx = get_free_block();
    if (new_block_idx == -1) {
        std::cerr << "GC Fatal Error: No free block for fallback strategy!" << std::endl;
        return false;
    }

    for (int i = 0; i < PAGES_PER_BLOCK; ++i) {
        if (victim_block.pages[i].state == PageState::VALID) {
            int lpn = victim_block.pages[i].logical_page_number;
            PPA new_ppa = {new_block_idx, nand_.blocks[new_block_idx].current_page};
            nand_.write(new_ppa.block, new_ppa.page, lpn);
            l2p_mapping_[lpn] = new_ppa;
        }
    }
    nand_.erase(victim_idx);

    // 새 블록을 열었으니, 이 블록을 새로운 Active Block으로 삼는 것이 효율적
    active_block_ = new_block_idx;
    
    return true;
}


int FTL::find_victim_block_greedy() {
    int victim_block = -1;
    int max_invalid_pages = -1;

    for (int i = 0; i < NUM_BLOCKS; ++i) {
        if (i == active_block_) continue;
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
        if (i == active_block_) continue;
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
        if (nand_.blocks[i].current_page == 0) {
            return i;
        }
    }
    return -1;
}

// 간단한 마모 평준화 로직
void FTL::wear_leveling() {
    int min_erase_count = nand_.blocks[0].erase_count;
    int min_erase_idx = 0;

    for (int i = 1; i < NUM_BLOCKS; ++i) {
        if (nand_.blocks[i].erase_count < min_erase_count) {
            min_erase_count = nand_.blocks[i].erase_count;
            min_erase_idx = i;
        }
    }

    // Active Block과 가장 마모가 덜 된 블록의 지우기 횟수 차이가 특정 임계값을 넘으면
    const int WEAR_LEVELING_THRESHOLD = 5;
    if (nand_.blocks[active_block_].erase_count > min_erase_count + WEAR_LEVELING_THRESHOLD) {
        int free_block = get_free_block();
        if (free_block != -1) {
            // 마모가 덜 된 블록의 유효 데이터를 다른 빈 블록으로 옮기고
            Block& min_worn_block = nand_.blocks[min_erase_idx];
             for (int i = 0; i < PAGES_PER_BLOCK; ++i) {
                if (min_worn_block.pages[i].state == PageState::VALID) {
                    int lpn = min_worn_block.pages[i].logical_page_number;
                    PPA new_ppa = {free_block, nand_.blocks[free_block].current_page};
                    nand_.write(new_ppa.block, new_ppa.page, lpn);
                    l2p_mapping_[lpn] = new_ppa;
                }
            }
            // 마모가 덜 된 블록을 지워서 자주 사용되도록 유도
            nand_.erase(min_erase_idx);
        }
    }
}

// WAF 계산
double FTL::getWAF() const {
    if (user_writes_ == 0) {
        return 0.0;
    }
    // WAF = (전체 NAND 쓰기 횟수) / (호스트 쓰기 횟수)
    return static_cast<double>(nand_.get_nand_writes()) / user_writes_;
}

// 모든 블록의 상태를 상세히 출력하는 디버그 함수
void FTL::print_debug_state() {
    std::cout << "\n--- NAND FLASH DEBUG STATE ---" << std::endl;
    std::cout << "Active Block: " << active_block_ << std::endl;
    std::cout << "Free Blocks Count: " << count_free_blocks() << std::endl;
    std::cout << std::left << std::setw(8) << "Block"
              << std::setw(8) << "Valid"
              << std::setw(10) << "Invalid"
              << std::setw(8) << "Current"
              << std::setw(8) << "Erase" << std::endl;
    std::cout << "-----------------------------------------------" << std::endl;
    for (int i = 0; i < NUM_BLOCKS; ++i) {
        // 데이터가 있거나, 방금 채워진 블록, Active Block만 출력
        if (nand_.blocks[i].valid_pages > 0 || nand_.blocks[i].invalid_pages > 0 || nand_.blocks[i].current_page > 0 || i == active_block_) {
             std::cout << std::left << std::setw(8) << i
                       << std::setw(8) << nand_.blocks[i].valid_pages
                       << std::setw(10) << nand_.blocks[i].invalid_pages
                       << std::setw(8) << nand_.blocks[i].current_page
                       << std::setw(8) << nand_.blocks[i].erase_count << std::endl;
        }
    }
    std::cout << "-----------------------------------------------\n" << std::endl;
}