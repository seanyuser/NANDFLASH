// 덮어쓸 파일: FTL.cpp

#include "FTL.h"
#include <iostream>
#include <iomanip> 
#include <algorithm>

FTL::FTL() : user_writes_(0), user_reads_(0) {
    for (int i = 0; i < NUM_BLOCKS; ++i) {
        nand_.erase(i);
    }
    hot_active_block_ = 0; 
    cold_active_block_ = 1;
    // ✅ closed_hot_blocks_ 와 closed_cold_blocks_ 는 자동으로 비어있게 초기화됨
}

// ... (write, read 함수는 기존과 동일) ...
bool FTL::write(int lpn) {
    user_writes_++;
    
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


// ✅ [수정됨] 꽉 찬 블록을 "레이블" 리스트에 추가하는 로직
bool FTL::get_new_page(PPA& ppa, int lpn) {
    bool is_hot = (lpn_write_counts_.count(lpn) && lpn_write_counts_[lpn] > HOT_LPN_THRESHOLD);

    if (is_hot) {
        // --- Hot 데이터 경로 ---
        if (nand_.blocks[hot_active_block_].current_page >= PAGES_PER_BLOCK) {
            
            // ✅ [추가] 꽉 찬 Hot 블록을 "레이블" 리스트에 추가
            closed_hot_blocks_.push_back(hot_active_block_);

            hot_active_block_ = get_free_block(); 
            if (hot_active_block_ == -1) {
                std::cerr << "Fatal Error in get_new_page: No free block for HOT writes." << std::endl;
                return false;
            }
        }
        ppa = {hot_active_block_, nand_.blocks[hot_active_block_].current_page};
    } else {
        // --- Cold 데이터 경로 ---
        if (nand_.blocks[cold_active_block_].current_page >= PAGES_PER_BLOCK) {
            
            // ✅ [추가] 꽉 찬 Cold 블록을 "레이블" 리스트에 추가
            closed_cold_blocks_.push_back(cold_active_block_);

            cold_active_block_ = get_free_block(); 
            if (cold_active_block_ == -1) {
                std::cerr << "Fatal Error in get_new_page: No free block for COLD writes." << std::endl;
                return false;
            }
        }
        ppa = {cold_active_block_, nand_.blocks[cold_active_block_].current_page};
    }
    return true;
}

// ... (count_free_blocks 함수는 기존과 동일) ...
int FTL::count_free_blocks() {
    int count = 0;
    for (int i = 0; i < NUM_BLOCKS; i++) {
        if (i == hot_active_block_ || i == cold_active_block_) continue;
        
        // ✅ [개선] 닫힌 블록 리스트에 있는 블록도 Free가 아님
        // (사실 current_page == 0 조건이 이미 이 역할을 하지만,
        //  get_free_block이 이 리스트를 확인하도록 수정하는 것이 더 안전함.
        //  일단은 get_free_block이 정확하다고 가정하고, 이 함수는 그대로 둠.)
        
        if (nand_.blocks[i].current_page == 0) {
            // (참고: 이 블록이 closed_hot/cold_blocks_ 리스트에 있다면 논리 오류)
            count++;
        }
    }
    return count;
}


// ✅ [수정됨] GC 로직 (find_victim_block_smart 호출)
bool FTL::garbage_collect() {
    // ✅ [변경] "Greedy" 대신 "Smart" 함수 호출
    int victim_idx = find_victim_block_smart(); 

    if (victim_idx == -1) {
        // (모든 블록이 꽉 찼지만 지울 게 없는 드문 경우)
        std::cerr << "GC Warning: No victim block found (All blocks might be full and valid)." << std::endl;
        return true; 
    }

    Block& victim_block = nand_.blocks[victim_idx];

    // (이하 GC의 복사 로직은 기존과 100% 동일)
    
    // 1. 희생양 블록을 스캔하여 복사할 Hot/Cold 페이지 수 계산
    int hot_pages_to_copy = 0;
    int cold_pages_to_copy = 0;
    for (int i = 0; i < PAGES_PER_BLOCK; ++i) {
        if (victim_block.pages[i].state == PageState::VALID) {
            int lpn = victim_block.pages[i].logical_page_number;
            if (lpn_write_counts_.count(lpn) && lpn_write_counts_[lpn] > HOT_LPN_THRESHOLD) {
                hot_pages_to_copy++;
            } else {
                cold_pages_to_copy++;
            }
        }
    }
    // ... (디버그 출력 주석) ...

    // 2. "스마트 병합" 시도
    Block& hot_active = nand_.blocks[hot_active_block_];
    Block& cold_active = nand_.blocks[cold_active_block_];
    bool can_merge_hot = (PAGES_PER_BLOCK - hot_active.current_page) >= hot_pages_to_copy;
    bool can_merge_cold = (PAGES_PER_BLOCK - cold_active.current_page) >= cold_pages_to_copy;

    if (can_merge_hot && can_merge_cold) {
        // --- 전략 1: "스마트 병합" 성공 ---
        for (int i = 0; i < PAGES_PER_BLOCK; ++i) {
            if (victim_block.pages[i].state == PageState::VALID) {
                int lpn = victim_block.pages[i].logical_page_number;
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
    int new_hot_block = -1;
    int new_cold_block = -1;
    int original_hot_active = hot_active_block_;
    int original_cold_active = cold_active_block_;
    // ... (기존의 '블록 낭비 방지' 로직은 그대로 사용) ...
    if (hot_pages_to_copy > 0) {
        new_hot_block = get_free_block();
        if (new_hot_block == -1) return false;
        hot_active_block_ = new_hot_block; 
    }
    if (cold_pages_to_copy > 0) {
        new_cold_block = get_free_block();
        hot_active_block_ = original_hot_active; 
        if (new_cold_block == -1) return false; 
        cold_active_block_ = new_cold_block;
    }
    hot_active_block_ = original_hot_active;
    cold_active_block_ = original_cold_active;

    for (int i = 0; i < PAGES_PER_BLOCK; ++i) {
        if (victim_block.pages[i].state == PageState::VALID) {
            int lpn = victim_block.pages[i].logical_page_number;
            bool is_hot = (lpn_write_counts_.count(lpn) && lpn_write_counts_[lpn] > HOT_LPN_THRESHOLD);
            PPA new_ppa;
            if (is_hot) {
                new_ppa = {new_hot_block, nand_.blocks[new_hot_block].current_page};
            } else {
                new_ppa = {new_cold_block, nand_.blocks[new_cold_block].current_page};
            }
            nand_.write(new_ppa.block, new_ppa.page, lpn);
            l2p_mapping_[lpn] = new_ppa;
        }
    }
    nand_.erase(victim_idx);
    if (new_hot_block != -1) hot_active_block_ = new_hot_block;
    if (new_cold_block != -1) cold_active_block_ = new_cold_block;
    
    return true;
}


// ✅ [완전히 새로 구현됨] Hot 블록 리스트를 우선 탐색하는 "Smart" GC
    int FTL::find_victim_block_smart() {
    int victim_block = -1;
    
    // --- 전략 0: Smart (기존 로직 - invalid 페이지 최대화) ---
    if (gc_victim_strategy == 0) { 
        int max_invalid_pages = -1;
        int vector_index_to_erase = -1;

        // 우선순위 1: Hot 리스트 스캔
        for (int i = 0; i < closed_hot_blocks_.size(); ++i) {
            int block_idx = closed_hot_blocks_[i];
            if (block_idx == hot_active_block_ || block_idx == cold_active_block_) continue; 
            if (nand_.blocks[block_idx].invalid_pages > max_invalid_pages) {
                max_invalid_pages = nand_.blocks[block_idx].invalid_pages;
                victim_block = block_idx;
                vector_index_to_erase = i;
            }
        }
        if (max_invalid_pages > 0) {
            closed_hot_blocks_.erase(closed_hot_blocks_.begin() + vector_index_to_erase);
            return victim_block;
        }

        // 우선순위 2: Cold 리스트 스캔
        max_invalid_pages = -1; 
        vector_index_to_erase = -1;
        for (int i = 0; i < closed_cold_blocks_.size(); ++i) {
             int block_idx = closed_cold_blocks_[i];
            if (block_idx == hot_active_block_ || block_idx == cold_active_block_) continue; 
            if (nand_.blocks[block_idx].invalid_pages > max_invalid_pages) {
                max_invalid_pages = nand_.blocks[block_idx].invalid_pages;
                victim_block = block_idx;
                vector_index_to_erase = i;
            }
        }
        if (max_invalid_pages > 0) {
            closed_cold_blocks_.erase(closed_cold_blocks_.begin() + vector_index_to_erase);
            return victim_block;
        }
        
        // 우선순위 3: Fallback (기존 로직)
        if (!closed_cold_blocks_.empty()) {
            victim_block = closed_cold_blocks_[0];
            closed_cold_blocks_.erase(closed_cold_blocks_.begin());
            return victim_block;
        }
        if (!closed_hot_blocks_.empty()) {
            victim_block = closed_hot_blocks_[0];
            closed_hot_blocks_.erase(closed_hot_blocks_.begin());
            return victim_block;
        }
        return -1; // 희생양 없음
    } 
    // --- 전략 1: Simple (사용자 제안 - 가장 오래된 Hot 블록 우선) ---
    else if (gc_victim_strategy == 1) { 
        // 우선순위 1: Hot 리스트의 첫 번째 블록 선택 (존재한다면)
        if (!closed_hot_blocks_.empty()) {
            victim_block = closed_hot_blocks_[0]; 
            // (Active Block인지 확인하는 안전장치 추가 가능)
            if (victim_block != hot_active_block_ && victim_block != cold_active_block_) {
                 closed_hot_blocks_.erase(closed_hot_blocks_.begin());
                 return victim_block;
            }
            // (만약 Active Block이라면, 리스트에서 제거하고 다음 우선순위로)
             closed_hot_blocks_.erase(closed_hot_blocks_.begin()); 
        }

        // 우선순위 2: Cold 리스트에서 invalid 최대 블록 탐색 (Hot이 없거나 Active였을 경우)
        int max_invalid_pages = -1;
        int vector_index_to_erase = -1;
        for (int i = 0; i < closed_cold_blocks_.size(); ++i) {
             int block_idx = closed_cold_blocks_[i];
            if (block_idx == hot_active_block_ || block_idx == cold_active_block_) continue; 
            if (nand_.blocks[block_idx].invalid_pages > max_invalid_pages) {
                max_invalid_pages = nand_.blocks[block_idx].invalid_pages;
                victim_block = block_idx;
                vector_index_to_erase = i;
            }
        }
         if (max_invalid_pages >= 0) { // ✅ Cold 블록은 invalid가 0이라도 선택 가능
            // (Fallback: Cold 리스트에 블록이 있고 invalid=0인 경우 첫번째 선택)
             if (victim_block == -1 && !closed_cold_blocks_.empty()) {
                 victim_block = closed_cold_blocks_[0];
                 vector_index_to_erase = 0;
             }
             if (victim_block != -1) {
                closed_cold_blocks_.erase(closed_cold_blocks_.begin() + vector_index_to_erase);
                return victim_block;
             }
        }
        
        // 우선순위 3: Fallback (Cold 리스트도 비었을 경우, 남은 Hot 블록 선택)
        // (위에서 첫번째 Hot 블록이 Active여서 제거만 된 경우 여기에 해당)
        if (!closed_hot_blocks_.empty()) {
            victim_block = closed_hot_blocks_[0];
            closed_hot_blocks_.erase(closed_hot_blocks_.begin());
            return victim_block;
        }

        return -1; // 정말 희생양 없음
    }
    // --- 잘못된 전략 값 ---
    else {
        std::cerr << "Error: Invalid gc_victim_strategy value (" << gc_victim_strategy << ")" << std::endl;
        return -1;
    }
}


int FTL::get_free_block() {
    for (int i = 0; i < NUM_BLOCKS; i++) {
        // ✅ Active 블록은 Free가 아님
        if (i == hot_active_block_ || i == cold_active_block_) continue;
        
        // (참고: 닫힌 블록 리스트에 있는 블록들은 current_page가 0이 아니므로
        //  이 로직에 의해 자동으로 걸러집니다. 따라서 이 함수는 수정이 불필요.)
        
        if (nand_.blocks[i].current_page == 0) {
            return i;
        }
    }
    return -1; // 빈 블록 없음
}

// ... (wear_leveling, getWAF, print_debug_state 함수는 기존과 동일) ...
void FTL::wear_leveling() {
    // (기존 코드)
    int min_erase_count = nand_.blocks[0].erase_count;
    int min_erase_idx = 0;
    for (int i = 1; i < NUM_BLOCKS; ++i) {
        if (nand_.blocks[i].erase_count < min_erase_count) {
            min_erase_count = nand_.blocks[i].erase_count;
            min_erase_idx = i;
        }
    }
    const int WEAR_LEVELING_THRESHOLD = 5;
    if (nand_.blocks[hot_active_block_].erase_count > min_erase_count + WEAR_LEVELING_THRESHOLD) {
        int free_block = get_free_block();
        if (free_block != -1) {
            Block& min_worn_block = nand_.blocks[min_erase_idx];
             for (int i = 0; i < PAGES_PER_BLOCK; ++i) {
                if (min_worn_block.pages[i].state == PageState::VALID) {
                    int lpn = min_worn_block.pages[i].logical_page_number;
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
    std::cout << "Hot Active Block: " << hot_active_block_ << std::endl;
    std::cout << "Cold Active Block: " << cold_active_block_ << std::endl;
    std::cout << "Free Blocks Count: " << count_free_blocks() << std::endl;
    
    // ✅ [추가] 닫힌 블록 리스트 크기 출력
    std::cout << "Closed Hot Blocks (Label): " << closed_hot_blocks_.size() << std::endl;
    std::cout << "Closed Cold Blocks (Label): " << closed_cold_blocks_.size() << std::endl;

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