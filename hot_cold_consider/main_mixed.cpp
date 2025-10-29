// 덮어쓸 파일: main_mixed.cpp (hot_cold_consider 폴더 안의 파일)

#include <iostream>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <numeric>
#include <algorithm>
#include <iomanip>
#include <string>   // ✅ stringstream 사용 위해 추가
#include <sstream>  // ✅ stringstream 사용 위해 추가
#include "FTL.h"

// (GC 전략 변수 사용 원하시면 여기에 추가: int gc_victim_strategy = 0;)

int main() {
    srand(time(0));

    // (GC 전략 사용 시 출력 코드 추가)
    // std::cout << "Using GC Victim Strategy: " << ... << std::endl;

    const int TOTAL_OPERATIONS = 50000;
    const int WRITE_PERCENTAGE = 80;

    // ✅ NUM_SIMULATIONS = 1로 변경
    const int NUM_SIMULATIONS = 1;

    // --- ✅ [수정] 코드 내에서 직접 관찰 범위 설정 ---
    const int range_start = 1000; // 시작 operation 번호
    const int range_stop  = 2000; // 끝 operation 번호 (이 번호는 포함 안 됨)
    const int range_step  = 100;  // 간격
    // -------------------------------------------------

    if (range_start < 0 || range_stop <= range_start || range_step <= 0 || range_stop > TOTAL_OPERATIONS) {
        std::cerr << "Invalid range input. Ensure start >= 0, stop > start, step > 0, and stop <= TOTAL_OPERATIONS." << std::endl;
        return 1;
    }
    std::cout << "Observing operations from " << range_start << " up to (but not including) "
              << range_stop << " with a step of " << range_step << std::endl;
    // ----------------------------------------

    // --- (워크로드 설정: 90/10 확률 유지) ---
    const double HOT_ZONE_PERCENTAGE = 0.10;
    const double HOT_ACCESS_PERCENTAGE = 0.90;
    const int HOT_ZONE_LPNS = static_cast<int>(NUM_LOGICAL_PAGES * HOT_ZONE_PERCENTAGE);
    const int COLD_ZONE_LPNS = NUM_LOGICAL_PAGES - HOT_ZONE_LPNS;
    // ------------------------------------

    std::vector<double> final_wafs;

    std::cout << "Starting " << NUM_SIMULATIONS << " SSD simulation (Observer Mode)..." << std::endl;
    std::cout << "----------------------------------------" << std::endl;

    for (int sim = 0; sim < NUM_SIMULATIONS; ++sim) {
        FTL ftl;

        for (int i = 0; i < TOTAL_OPERATIONS; ++i) {

            // ✅ --- [추가] 지정된 간격마다 상태 출력 ---
            if (i >= range_start && i < range_stop && (i - range_start) % range_step == 0) {
                std::cout << "\n--- Operation #" << i << " State ---" << std::endl;
                int current_hot_active = ftl.get_hot_active_block();
                int current_cold_active = ftl.get_cold_active_block();
                const NandFlash& nand = ftl.get_nand_flash(); // NandFlash 객체 참조 가져오기

                std::cout << "Hot Active: " << current_hot_active
                          << ", Cold Active: " << current_cold_active << std::endl;
                std::cout << "------------------------------------------------------------" << std::endl;
                std::cout << std::left
                          << std::setw(8) << "Block"
                          << std::setw(10) << "Hot(V)" // Valid Hot
                          << std::setw(10) << "Cold(V)" // Valid Cold
                          << std::setw(10) << "Current"
                          << std::setw(8) << "Valid"
                          << std::setw(10) << "Invalid" << std::endl;
                std::cout << "------------------------------------------------------------" << std::endl;

                for (int block_idx = 0; block_idx < 5; ++block_idx) { // 최초 5개 블록
                    int hot_count, cold_count;
                    ftl.get_block_hot_cold_counts(block_idx, hot_count, cold_count);
                    const Block& block = nand.blocks[block_idx];

                    std::cout << std::left
                              << std::setw(8) << block_idx
                              << std::setw(10) << hot_count
                              << std::setw(10) << cold_count
                              << std::setw(10) << block.current_page
                              << std::setw(8) << block.valid_pages
                              << std::setw(10) << block.invalid_pages;

                    // Active 블록 표시
                    if (block_idx == current_hot_active) std::cout << " <- HOT";
                    if (block_idx == current_cold_active) std::cout << " <- COLD";
                    std::cout << std::endl;
                }
                std::cout << "------------------------------------------------------------" << std::endl;
            }
            // -------------------------------------------

            // (워크로드 실행 로직은 90/10 확률 유지)
            if ((rand() % 100) < WRITE_PERCENTAGE) {
                int lpn;
                if ((rand() % 100) < (HOT_ACCESS_PERCENTAGE * 100)) {
                    lpn = rand() % HOT_ZONE_LPNS;
                } else {
                    lpn = (rand() % COLD_ZONE_LPNS) + HOT_ZONE_LPNS;
                }

                if (!ftl.write(lpn)) {
                    std::cout << "\n--- Simulation stopped due to a fatal error at operation " << i + 1 << " ---" << std::endl;
                    // 디버그 상태 출력 후 종료
                    ftl.print_debug_state();
                    break;
                }
            } else {
                int read_lpn = rand() % NUM_LOGICAL_PAGES;
                ftl.read(read_lpn);
            }
        } // End of TOTAL_OPERATIONS loop

        final_wafs.push_back(ftl.getWAF());

        std::cout << "\nSimulation " << sim + 1 << "/" << NUM_SIMULATIONS << " completed." << std::endl;

    } // End of NUM_SIMULATIONS loop

    // ✅ 최종 WAF만 출력하도록 수정
    std::cout << "----------------------------------------" << std::endl;
    if (!final_wafs.empty()){
        std::cout << std::fixed << std::setprecision(5);
        std::cout << "Final WAF after " << TOTAL_OPERATIONS << " operations: " << final_wafs[0] << std::endl;
    } else {
        std::cout << "Simulation did not complete successfully." << std::endl;
    }
    std::cout << "----------------------------------------" << std::endl;

    return 0;
}