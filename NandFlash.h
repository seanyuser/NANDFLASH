#ifndef NANDFLASH_H
#define NANDFLASH_H

#include <vector>
#include <iostream>

// NAND 플래시 메모리 규격 상수
const int NUM_BLOCKS = 128;       // 전체 블록 개수
const int PAGES_PER_BLOCK = 64;   // 블록 당 페이지 개수

// 페이지의 상태를 나타내는 열거형
enum class PageState {
    FREE,       // 비어있는 상태
    VALID,      // 유효한 데이터가 저장된 상태
    INVALID     // 오래된, 무효화된 데이터가 저장된 상태
};

// 페이지 구조체
struct Page {
    PageState state;
    int logical_page_number; // 이 페이지에 저장된 데이터의 논리 주소(LPN)
};

// 블록 구조체
struct Block {
    std::vector<Page> pages;
    int erase_count;         // 블록이 지워진 횟수 (Wear Leveling에 사용)
    int valid_pages;         // 블록 내 유효한 페이지 개수
    int invalid_pages;       // 블록 내 무효화된 페이지 개수
    int current_page;        // 현재 블록에서 다음 쓰기가 이루어질 페이지 인덱스

    Block() : pages(PAGES_PER_BLOCK), erase_count(0), valid_pages(0), invalid_pages(0), current_page(0) {
        for (int i = 0; i < PAGES_PER_BLOCK; ++i) {
            pages[i].state = PageState::FREE;
            pages[i].logical_page_number = -1;
        }
    }
};

// NAND 플래시 메모리 시뮬레이션 클래스
class NandFlash {
public:
    NandFlash();

    // NAND 기본 동작 함수
    bool write(int block, int page, int lpn);
    bool read(int block, int page);
    bool erase(int block);

    // 통계 정보 GETTER
    long long get_nand_writes() const { return nand_writes_; }
    long long get_nand_erases() const { return nand_erases_; }

    // FTL에서 블록 정보에 직접 접근하기 위한 public 멤버
    std::vector<Block> blocks;

private:
    long long nand_writes_; // NAND에 직접 쓰기 작업이 발생한 총 횟수
    long long nand_erases_; // 블록 지우기 작업이 발생한 총 횟수
};

#endif // NANDFLASH_H