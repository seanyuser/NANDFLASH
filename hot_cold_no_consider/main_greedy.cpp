#include <iostream>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <numeric>
#include <algorithm>
#include <iomanip>
#include "FTL_Greedy.h" // ✅ "단순 FTL" 헤더를 포함

int main() {
    srand(time(0));

    // (모든 설정값은 main_mixed.cpp와 100% 동일)
    const int TOTAL_OPERATIONS = 50000;
    const int WRITE_PERCENTAGE = 80;
    const int NUM_SIMULATIONS = 1000; 

    const double HOT_ZONE_PERCENTAGE = 0.10; 
    const int HOT_ZONE_LPNS = static_cast<int>(NUM_LOGICAL_PAGES * HOT_ZONE_PERCENTAGE);
    const int COLD_ZONE_LPNS = NUM_LOGICAL_PAGES - HOT_ZONE_LPNS;

    bool currently_writing_hot = true;
    int writes_remaining_in_burst = 0;

    std::vector<double> final_wafs;

    // ✅ "Greedy FTL"로 테스트한다는 것을 명시
    std::cout << "Starting " << NUM_SIMULATIONS << " SSD simulations (Mixed Block Workload on Greedy FTL)..." << std::endl;
    std::cout << "Total operations per simulation: " << TOTAL_OPERATIONS << std::endl;
    std::cout << "Workload: Bursty writes (Hot/Cold) designed to cause block contamination." << std::endl;
    std::cout << "----------------------------------------" << std::endl;

    for (int sim = 0; sim < NUM_SIMULATIONS; ++sim) {
        
        FTL_Greedy ftl; // ✅ "단순 FTL" 객체 생성
        
        currently_writing_hot = true;
        writes_remaining_in_burst = 0;

        for (int i = 0; i < TOTAL_OPERATIONS; ++i) {
            
            // (이하 워크로드 로직은 main_mixed.cpp와 100% 동일)
            if ((rand() % 100) < WRITE_PERCENTAGE) {
                
                if (writes_remaining_in_burst == 0) {
                    if (rand() % 2 == 0) {
                        currently_writing_hot = true;
                        writes_remaining_in_burst = (rand() % 10) + 25; // ✅ WAF 2.49 테스트와 동일하게 25
                    } else {
                        currently_writing_hot = false;
                        writes_remaining_in_burst = (rand() % 10) + 25; // ✅ WAF 2.49 테스트와 동일하게 25
                    }
                }
                
                int lpn;
                if (currently_writing_hot) {
                    lpn = rand() % HOT_ZONE_LPNS;
                } else {
                    lpn = (rand() % COLD_ZONE_LPNS) + HOT_ZONE_LPNS;
                }
                writes_remaining_in_burst--; 

                if (!ftl.write(lpn)) {
                    std::cout << "\n--- Simulation " << sim + 1 << " stopped due to a fatal error at operation " << i + 1 << " ---" << std::endl;
                    break;
                }
            } else {
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
    std::cout << "--- WAF Distribution Statistics (Greedy FTL) ---" << std::endl; // ✅

    // (통계 출력 로직은 100% 동일)
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