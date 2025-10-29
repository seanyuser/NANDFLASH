// 덮어쓸 파일: hot_cold_consider/main_mixed.cpp

#include <iostream>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <numeric>
#include <algorithm>
#include <iomanip>
#include "FTL.h" // ✅ Hot/Cold FTL 사용

int gc_victim_strategy = 0;

int main() {
    srand(time(0));
    
    std::cout << "Using GC Victim Strategy: " 
              << (gc_victim_strategy == 0 ? "Smart (Max Invalid)" : "Simple (Oldest Hot First)") 
              << std::endl;

    const int TOTAL_OPERATIONS = 50000;
    const int WRITE_PERCENTAGE = 80;
    const int NUM_SIMULATIONS = 300;

    // --- ✅ "90/10 확률" 워크로드 설정 ---
    const double HOT_ZONE_PERCENTAGE = 0.10; // LPN의 10%가 Hot
    const double HOT_ACCESS_PERCENTAGE = 0.90; // 쓰기의 90%가 Hot Zone에 집중

    const int HOT_ZONE_LPNS = static_cast<int>(NUM_LOGICAL_PAGES * HOT_ZONE_PERCENTAGE);
    const int COLD_ZONE_LPNS = NUM_LOGICAL_PAGES - HOT_ZONE_LPNS;
    // ------------------------------------

    // 🛑 "버스트" 쓰기 관련 변수 (currently_writing_hot, writes_remaining_in_burst) 삭제

    std::vector<double> final_wafs;

    std::cout << "Starting " << NUM_SIMULATIONS << " SSD simulations (90/10 Workload on Hot/Cold FTL)..." << std::endl;
    std::cout << "Total operations per simulation: " << TOTAL_OPERATIONS << std::endl;
    std::cout << "Workload: 90% of writes to 10% of LPNs (Testing Block Contamination)" << std::endl;
    std::cout << "----------------------------------------" << std::endl;

    for (int sim = 0; sim < NUM_SIMULATIONS; ++sim) {
        FTL ftl; // ✅ Hot/Cold FTL 객체 생성

        // 🛑 버스트 상태 변수 초기화 삭제

        for (int i = 0; i < TOTAL_OPERATIONS; ++i) {
            
            if ((rand() % 100) < WRITE_PERCENTAGE) {
                
                // 🛑 --- [시작] 버스트 로직 (if writes_remaining_in_burst) 삭제 ---
                
                // ✅ --- [시작] "90/10 확률" 로직으로 교체 ---
                int lpn;
                if ((rand() % 100) < (HOT_ACCESS_PERCENTAGE * 100)) {
                    // 90% 확률: Hot Zone (LPN 0 ~ HOT_ZONE_LPNS-1)을 타겟
                    lpn = rand() % HOT_ZONE_LPNS;
                } else {
                    // 10% 확률: Cold Zone (LPN HOT_ZONE_LPNS ~ 끝)을 타겟
                    lpn = (rand() % COLD_ZONE_LPNS) + HOT_ZONE_LPNS;
                }
                // 🛑 writes_remaining_in_burst--; // 삭제
                // ✅ --- [끝] "90/10 확률" 로직으로 교체 ---

                if (!ftl.write(lpn)) {
                    std::cout << "\n--- Simulation " << sim + 1 << " stopped due to a fatal error at operation " << i + 1 << " ---" << std::endl;
                    break;
                }
            } else {
                // 읽기 작업
                int read_lpn = rand() % NUM_LOGICAL_PAGES;
                ftl.read(read_lpn);
            }
        }

        final_wafs.push_back(ftl.getWAF());

        if ((sim + 1) % 100 == 0 || sim == NUM_SIMULATIONS - 1) {
            std::cout << "Simulation " << sim + 1 << "/" << NUM_SIMULATIONS << " completed." << std::endl;
        }
    }

    std::cout << "----------------------------------------" << std::endl;
    std::cout << "All " << NUM_SIMULATIONS << " simulations finished!" << std::endl;
    std::cout << "--- WAF Distribution Statistics (Hot/Cold FTL - 90/10) ---" << std::endl;

    if (!final_wafs.empty()) {
        double sum = std::accumulate(final_wafs.begin(), final_wafs.end(), 0.0);
        double average_waf = sum / final_wafs.size();
        double min_waf = *std::min_element(final_wafs.begin(), final_wafs.end());
        double max_waf = *std::max_element(final_wafs.begin(), final_wafs.end());

        std::cout << std::fixed << std::setprecision(5);
        std::cout << "Average WAF: " << average_waf << std::endl;
        std::cout << "Min WAF:     " << min_waf << std::endl;
        std::cout << "Max WAF:     " << max_waf << std::endl;
        std::cout << "\n(Data for " << final_wafs.size() << " successful runs collected)" << std::endl;
    }

    return 0;
}