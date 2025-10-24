#include <iostream>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <numeric>
#include <algorithm>
#include <iomanip>
#include "FTL.h"

int main() {
    srand(time(0));

    const int TOTAL_OPERATIONS = 50000;
    const int WRITE_PERCENTAGE = 80;
    const int NUM_SIMULATIONS = 1000;

    // --- ✅ "Mixed Block" 워크로드 설정 ---
    const double HOT_ZONE_PERCENTAGE = 0.10; // LPN의 10%가 Hot
    const int HOT_ZONE_LPNS = static_cast<int>(NUM_LOGICAL_PAGES * HOT_ZONE_PERCENTAGE);
    const int COLD_ZONE_LPNS = NUM_LOGICAL_PAGES - HOT_ZONE_LPNS;

    // "버스트(Burst)" 쓰기를 위한 상태 변수
    bool currently_writing_hot = true;
    int writes_remaining_in_burst = 0;
    // ------------------------------------

    std::vector<double> final_wafs;

    std::cout << "Starting " << NUM_SIMULATIONS << " SSD simulations (Mixed Block Workload)..." << std::endl;
    std::cout << "Total operations per simulation: " << TOTAL_OPERATIONS << std::endl;
    std::cout << "Workload: Bursty writes (Hot/Cold) designed to cause block contamination." << std::endl;
    std::cout << "----------------------------------------" << std::endl;

    for (int sim = 0; sim < NUM_SIMULATIONS; ++sim) {
        FTL ftl;
        currently_writing_hot = true;
        writes_remaining_in_burst = 0;

        for (int i = 0; i < TOTAL_OPERATIONS; ++i) {
            
            // 쓰기 작업일 때만 Hot/Cold 버스트 로직 적용
            if ((rand() % 100) < WRITE_PERCENTAGE) {
                
                // ✅ 버스트 상태 갱신
                if (writes_remaining_in_burst == 0) {
                    // 50/50 확률로 Hot/Cold 상태 변경
                    if (rand() % 2 == 0) {
                        currently_writing_hot = true;
                        // Hot Zone에 평균 30번의 버스트 쓰기
                        writes_remaining_in_burst = (rand() % 10) + 25; 
                    } else {
                        currently_writing_hot = false;
                        // Cold Zone에 평균 30번의 버스트 쓰기
                        writes_remaining_in_burst = (rand() % 10) + 95; 
                    }
                }
                
                int lpn;
                if (currently_writing_hot) {
                    // Hot Zone (LPN 0 ~ HOT_ZONE_LPNS-1)을 타겟
                    lpn = rand() % HOT_ZONE_LPNS;
                } else {
                    // Cold Zone (LPN HOT_ZONE_LPNS ~ 끝)을 타겟
                    lpn = (rand() % COLD_ZONE_LPNS) + HOT_ZONE_LPNS;
                }
                writes_remaining_in_burst--; // 버스트 횟수 차감

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
    std::cout << "--- WAF Distribution Statistics (Mixed Block) ---" << std::endl;

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