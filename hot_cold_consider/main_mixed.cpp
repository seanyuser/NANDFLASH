// 덮어쓸 파일: main_mixed.cpp

#include <iostream>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <numeric>
#include <algorithm>
#include <iomanip>
#include <string>
#include <sstream>
#include "FTL.h"    // FTL 클래스 헤더
#include "NandFlash.h" // NandFlash 클래스 헤더 (NUM_BLOCKS 상수 사용 위해)

// (GC 전략 변수 사용 원하시면 여기에 추가: int gc_victim_strategy = 0;)

int main() {
    srand(time(0));

    const int TOTAL_OPERATIONS = 200000;
    const int WRITE_PERCENTAGE = 80;

    // ✅ --- HOT_LPN_THRESHOLD 범위 설정 (파이썬 range(start, stop, step) 형식) ---
    const int threshold_start = 1;  // 시작 값 (포함)
    const int threshold_stop  = 11; // 끝 값 (미포함)
    const int threshold_step  = 1;  // 증가 간격
    // ---------------------------------------------------------------------------------

    // --- 관찰 Operation 범위 설정 (첫 번째 임계값에서만 디버그 출력에 사용) ---
    const int range_start = 5500;
    const int range_stop  = 5502;
    const int range_step  = 90;
    // ------------------------------------

    // --- 관찰 Block 범위 설정 (첫 번째 임계값에서만 디버그 출력에 사용) ---
    const int observe_block_start = 0; 
    const int observe_block_stop  = 2; 
    // ----------------------------------

    // ... (기존 범위 유효성 검사 및 설정 출력은 첫 번째 임계값에서만 실행되도록 main_loop 외부로 이동) ...
    if (range_start < 0 || range_stop <= range_start || range_step <= 0 || range_stop > TOTAL_OPERATIONS) {
        std::cerr << "Invalid operation range settings in code." << std::endl;
        return 1;
    }
    if (observe_block_start < 0 || observe_block_stop <= observe_block_start || observe_block_stop > NUM_BLOCKS) {
        std::cerr << "Invalid block range settings in code. Ensure start >= 0, stop > start, and stop <= NUM_BLOCKS (" << NUM_BLOCKS << ")." << std::endl;
        return 1;
    }
    
    // ----------------------------------------

    // --- (워크로드 설정: 90/10 확률 유지) ---
    const double HOT_ZONE_PERCENTAGE = 0.10;
    const double HOT_ACCESS_PERCENTAGE = 0.90;
    const int HOT_ZONE_LPNS = static_cast<int>(NUM_LOGICAL_PAGES * HOT_ZONE_PERCENTAGE);
    const int COLD_ZONE_LPNS = NUM_LOGICAL_PAGES - HOT_ZONE_LPNS;
    // ------------------------------------

    std::vector<std::pair<int, double>> waf_results; // 임계값과 WAF 저장
    bool is_first_run = true; // 첫 번째 실행인지 확인하는 플래그

    std::cout << "Starting SSD simulation to test HOT_LPN_THRESHOLD..." << std::endl;
    std::cout << "Testing Thresholds from " << threshold_start << " to " << threshold_stop << " (step " << threshold_step << ")" << std::endl;
    std::cout << "Total Operations per test: " << TOTAL_OPERATIONS << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    
    // ✅ --- HOT_LPN_THRESHOLD를 반복하는 메인 루프 ---
    for (int threshold = threshold_start; threshold < threshold_stop; threshold += threshold_step) {
        
        // FTL 객체를 새로 생성하고 현재 임계값을 전달
        FTL ftl(threshold); 
        
        // 첫 번째 실행일 경우에만 디버그 정보 출력
        if (is_first_run) {
            std::cout << "Observing operations from " << range_start << " to " << range_stop << " (step " << range_step << ") only for Threshold: " << threshold_start << std::endl;
            std::cout << "Observing blocks from " << observe_block_start << " to " << observe_block_stop - 1 << std::endl;
        }

        for (int i = 0; i < TOTAL_OPERATIONS; ++i) {

            // (상태 출력 로직: 첫 번째 임계값에서만 실행)
            if (is_first_run && i >= range_start && i < range_stop && (i - range_start) % range_step == 0) {
                // ... (기존의 상태 출력 코드) ...
                std::cout << "\n--- Operation #" << i << " State (Threshold: " << threshold << ") ---" << std::endl;
                int current_hot_active = ftl.get_hot_active_block();
                int current_cold_active = ftl.get_cold_active_block();
                const NandFlash& nand = ftl.get_nand_flash();
                std::cout << "Hot Active: " << current_hot_active << ", Cold Active: " << current_cold_active << std::endl;
                std::cout << "------------------------------------------------------------------" << std::endl;
                std::cout << std::left << std::setw(8) << "Block" << std::setw(10) << "Hot(V)" << std::setw(10) << "Cold(V)" << std::setw(10) << "Current" << std::setw(10) << "Valid" << std::setw(10) << "Invalid" << std::endl;
                std::cout << "------------------------------------------------------------------" << std::endl;
                for (int block_idx = observe_block_start; block_idx < observe_block_stop; ++block_idx) {
                    int hot_count, cold_count;
                    ftl.get_block_hot_cold_counts(block_idx, hot_count, cold_count);
                    const Block& block = nand.blocks[block_idx];
                    std::cout << std::left << std::setw(8) << block_idx << std::setw(10) << hot_count << std::setw(10) << cold_count << std::setw(10) << block.current_page << std::setw(10) << block.valid_pages << std::setw(10) << block.invalid_pages;
                    std::cout << std::endl;
                }
                std::cout << "------------------------------------------------------------------" << std::endl;
            }

            // (워크로드 실행 로직은 90/10 확률 유지)
            if ((rand() % 100) < WRITE_PERCENTAGE) {
                int lpn;
                if ((rand() % 100) < (HOT_ACCESS_PERCENTAGE * 100)) {
                    lpn = rand() % HOT_ZONE_LPNS;
                } else {
                    lpn = (rand() % COLD_ZONE_LPNS) + HOT_ZONE_LPNS;
                }
                if (!ftl.write(lpn)) {
                    std::cout << "\n--- Simulation stopped due to a fatal error at operation " << i + 1 << " (Threshold: " << threshold << ") ---" << std::endl;
                    ftl.print_debug_state();
                    break;
                }
            } else {
                int read_lpn = rand() % NUM_LOGICAL_PAGES;
                ftl.read(read_lpn);
            }
        } // End of TOTAL_OPERATIONS loop

        // 최종 WAF 저장
        double waf = ftl.getWAF();
        waf_results.push_back({threshold, waf});

        // ✅ --- [첫 번째 실행에만] 시뮬레이션 종료 후 상세 출력 ---
        if (is_first_run) {
            std::cout << "\n--- Threshold: " << threshold << " Final Detailed Logs (Only First Run) ---" << std::endl;
            
            // Erase Count 출력
            std::cout << "\n--- Final Erase Counts per Block (After " << TOTAL_OPERATIONS << " ops) ---" << std::endl;
            const NandFlash& nand = ftl.get_nand_flash();
            for (int i = 0; i < NUM_BLOCKS; ++i) {
                std::cout << "Blk " << std::setw(3) << i << ": " << std::setw(5) << nand.blocks[i].erase_count << " | ";
                if ((i + 1) % 8 == 0 || i == NUM_BLOCKS - 1) {
                    std::cout << std::endl;
                }
            }

            // LPN 쓰기 횟수 출력
            std::cout << "\n--- Final LPN Write Counts (After " << TOTAL_OPERATIONS << " ops) ---" << std::endl;
            const std::map<int, int>& lpn_counts = ftl.get_lpn_write_counts();
            for (int lpn = 0; lpn < NUM_LOGICAL_PAGES; ++lpn) {
                int count = 0;
                if (lpn_counts.count(lpn)) {
                    count = lpn_counts.at(lpn);
                }
                std::cout << "LPN " << std::setw(4) << lpn << ": " << std::setw(5) << count << " | ";
                if ((lpn + 1) % 6 == 0 || lpn == NUM_LOGICAL_PAGES - 1) {
                    std::cout << std::endl;
                }
            }
            std::cout << "--------------------------------------------------------" << std::endl;
            is_first_run = false; // 플래그 해제
        }
        
        std::cout << "Threshold " << std::setw(3) << threshold << " completed. WAF: " << std::fixed << std::setprecision(5) << waf << std::endl;


    } // End of THRESHOLD loop

    // ✅ --- 최종 결과 요약 테이블 출력 ---
    std::cout << "\n========================================" << std::endl;
    std::cout << "          HOT_LPN_THRESHOLD vs WAF       " << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::left << std::setw(15) << "Threshold" << " | " << std::right << std::setw(10) << "Final WAF" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    
    std::cout << std::fixed << std::setprecision(5);
    for (const auto& result : waf_results) {
        std::cout << std::left << std::setw(15) << result.first << " | " << std::right << std::setw(10) << result.second << std::endl;
    }
    std::cout << "========================================" << std::endl;

    return 0;
}