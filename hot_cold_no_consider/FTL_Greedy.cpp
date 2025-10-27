#include "FTL_Greedy.h"
#include <iostream>
#include <iomanip> 
#include <algorithm>

FTL_Greedy::FTL_Greedy() : user_writes_(0), user_reads_(0) {
    for (int i = 0; i < NUM_BLOCKS; ++i) {
        nand_.erase(i);
    }
    // ✅ Active Block 0번 하나로 초기화
    active_block_ = 0; 
}

bool FTL_Greedy::write(int lpn) {
    user_writes_++;
    
    // ✅ LPN 쓰기 횟수 "학습" 로직 없음

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
    // ✅ LPN을 전달하지 않는 단순 할당
    if (!get_new_page(new_ppa)) {
        std::cerr << "Write failed because get_new_page failed." << std::endl;
        print_debug_state();
        return false;
    }
    
    nand_.write(new_ppa.block, new_ppa.page, lpn);
    l2p_mapping_[lpn] = new_ppa;

    return true;
}

void FTL_Greedy::read(int lpn) {
    user_reads_++;
    if (l2p_mapping_.count(lpn)) {
        PPA ppa = l2p_mapping_[lpn];
        nand_.read(ppa.block, ppa.page);
    }
}

// ✅ "온도" 판단 없는 단순 페이지 할당
bool FTL_Greedy::get_new_page(PPA& ppa) {
    if (nand_.blocks[active_block_].current_page >= PAGES_PER_BLOCK) {
        active_block_ = get_free_block(); // 블록이 꽉 차면 새 블록 할당
        if (active_block_ == -1) {
            std::cerr << "Fatal Error in get_new_page: No free block for writes." << std::endl;
            return false;
        }
    }
    ppa = {active_block_, nand_.blocks[active_block_].current_page};
    return true;
}

int FTL_Greedy::count_free_blocks() {
    int count = 0;
    for (int i = 0; i < NUM_BLOCKS; i++) {
        // ✅ Active Block 하나만 예비 블록에서 제외
        if (i == active_block_) continue;
        if (nand_.blocks[i].current_page == 0) {
            count++;
        }
    }
    return count;
}


// ✅ "Greedy GC" (단순 GC) 로직
bool FTL_Greedy::garbage_collect() {
    int victim_idx = find_victim_block_greedy();
    if (victim_idx == -1) {
        return true; 
    }

    Block& victim_block = nand_.blocks[victim_idx];

    // ✅ Hot/Cold 구분 없이 모든 Valid 페이지 수 계산
    int valid_pages_to_copy = victim_block.valid_pages;

    // 1. "병합" 시도: Active 블록에 공간이 있는지 확인
    Block& active = nand_.blocks[active_block_];
    bool can_merge = (PAGES_PER_BLOCK - active.current_page) >= valid_pages_to_copy;

    if (can_merge) {
        // --- 전략 1: "병합" (Active Block에 복사) ---
        for (int i = 0; i < PAGES_PER_BLOCK; ++i) {
            if (victim_block.pages[i].state == PageState::VALID) {
                int lpn = victim_block.pages[i].logical_page_number;
                PPA new_ppa = {active_block_, active.current_page};
                nand_.write(new_ppa.block, new_ppa.page, lpn);
                l2p_mapping_[lpn] = new_ppa;
            }
        }
        nand_.erase(victim_idx);
        return true;
    }

    // --- 전략 2: "복사" (새 블록에 복사) ---
    int new_block = get_free_block();
    if (new_block == -1) {
        std::cerr << "GC Fatal Error: Not enough free blocks for GC copy!" << std::endl;
        return false;
    }

    for (int i = 0; i < PAGES_PER_BLOCK; ++i) {
        if (victim_block.pages[i].state == PageState::VALID) {
            int lpn = victim_block.pages[i].logical_page_number;
            PPA new_ppa = {new_block, nand_.blocks[new_block].current_page};
            nand_.write(new_ppa.block, new_ppa.page, lpn);
            l2p_mapping_[lpn] = new_ppa;
        }
    }
    
    nand_.erase(victim_idx);
    active_block_ = new_block; // 새 블록을 Active Block으로 지정
    
    return true;
}


int FTL_Greedy::find_victim_block_greedy() {
    int victim_block = -1;
    int max_invalid_pages = -1;

    for (int i = 0; i < NUM_BLOCKS; ++i) {
        // ✅ Active Block 하나만 GC 대상에서 제외
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

int FTL_Greedy::get_free_block() {
    for (int i = 0; i < NUM_BLOCKS; i++) {
        // ✅ Active Block 하나만 Free에서 제외
        if (i == active_block_) continue;
        if (nand_.blocks[i].current_page == 0) {
            return i;
        }
    }
    return -1;
}

double FTL_Greedy::getWAF() const {
    if (user_writes_ == 0) {
        return 0.0;
    }
    return static_cast<double>(nand_.get_nand_writes()) / user_writes_;
}

void FTL_Greedy::print_debug_state() {
    std::cout << "\n--- NAND FLASH DEBUG STATE (Greedy FTL) ---" << std::endl;
    std::cout << "Active Block: " << active_block_ << std::endl;
    std::cout << "Free Blocks Count: " << count_free_blocks() << std::endl;
    std::cout << std::left << std::setw(8) << "Block"
              << std::setw(8) << "Valid"
              << std::setw(10) << "Invalid"
              << std::setw(8) << "Current"
              << std::setw(8) << "Erase" << std::endl;
    std::cout << "-----------------------------------------------" << std::endl;
    for (int i = 0; i < NUM_BLOCKS; ++i) {
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