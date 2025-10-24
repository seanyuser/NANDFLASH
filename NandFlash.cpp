#include "NandFlash.h"

NandFlash::NandFlash() : blocks(NUM_BLOCKS), nand_writes_(0), nand_erases_(0) {}

// 특정 블록의 특정 페이지에 데이터를 쓰는 함수
bool NandFlash::write(int block_idx, int page_idx, int lpn) {
    if (block_idx >= NUM_BLOCKS || page_idx >= PAGES_PER_BLOCK) {
        std::cerr << "Error: 유효하지 않은 주소에 쓰기 시도." << std::endl;
        return false;
    }
    if (blocks[block_idx].pages[page_idx].state != PageState::FREE) {
        std::cerr << "Error: 비어있지 않은 페이지에 쓰기 시도." << std::endl;
        return false;
    }

    blocks[block_idx].pages[page_idx].state = PageState::VALID;
    blocks[block_idx].pages[page_idx].logical_page_number = lpn;
    blocks[block_idx].valid_pages++;
    blocks[block_idx].current_page++;
    nand_writes_++; // 물리적 쓰기 횟수 증가
    return true;
}

// 특정 페이지의 데이터를 읽는 함수 (시뮬레이션에서는 상태 확인만 수행)
bool NandFlash::read(int block_idx, int page_idx) {
    if (block_idx >= NUM_BLOCKS || page_idx >= PAGES_PER_BLOCK) {
        std::cerr << "Error: 유효하지 않은 주소에서 읽기 시도." << std::endl;
        return false;
    }
    return blocks[block_idx].pages[page_idx].state == PageState::VALID;
}

// 특정 블록 전체를 지우는 함수
bool NandFlash::erase(int block_idx) {
    if (block_idx >= NUM_BLOCKS) {
        std::cerr << "Error: 유효하지 않은 블록 지우기 시도." << std::endl;
        return false;
    }

    for (int i = 0; i < PAGES_PER_BLOCK; ++i) {
        blocks[block_idx].pages[i].state = PageState::FREE;
        blocks[block_idx].pages[i].logical_page_number = -1;
    }

    blocks[block_idx].erase_count++;
    blocks[block_idx].valid_pages = 0;
    blocks[block_idx].invalid_pages = 0;
    blocks[block_idx].current_page = 0;
    nand_erases_++; // 블록 지우기 횟수 증가
    return true;
}