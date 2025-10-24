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
    const int NUM_SIMULATIONS = 1000; // ✅ 1000번 반복

    // --- ✅ Hot/Cold 워크로드 설정 ---
    const double HOT_ZONE_PERCENTAGE = 0.10; // 전체 LPN의 10%가 Hot Zone
    const double HOT_ACCESS_PERCENTAGE = 0.90; // 전체 쓰기의 90%가 Hot Zone에 집중

    // Hot Zone과 Cold Zone의 LPN 개수 계산
    const int HOT_ZONE_LPNS = static_cast<int>(NUM_LOGICAL_PAGES * HOT_ZONE_PERCENTAGE);
    const int COLD_ZONE_LPNS = NUM_LOGICAL_PAGES - HOT_ZONE_LPNS;
    // ---------------------------------

    std::vector<double> final_wafs;

    std::cout << "Starting " << NUM_SIMULATIONS << " SSD simulations (Hot/Cold Workload)..." << std::endl;
    std::cout << "Total operations per simulation: " << TOTAL_OPERATIONS << std::endl;
    std::cout << "Hot Zone: 90% of writes go to 10% of LPNs (" << HOT_ZONE_LPNS << " pages)" << std::endl;
    std::cout << "Cold Zone: 10% of writes go to 90% of LPNs (" << COLD_ZONE_LPNS << " pages)" << std::endl;
    std::cout << "----------------------------------------" << std::endl;

    for (int sim = 0; sim < NUM_SIMULATIONS; ++sim) {
        FTL ftl;

        for (int i = 0; i < TOTAL_OPERATIONS; ++i) {
            
            // ✅ Hot/Cold 로직으로 LPN 결정
            int lpn;
            if ((rand() % 100) < (HOT_ACCESS_PERCENTAGE * 100)) {
                // 90% 확률: Hot Zone (LPN 0 ~ HOT_ZONE_LPNS-1)을 타겟
                lpn = rand() % HOT_ZONE_LPNS;
            } else {
                // 10% 확률: Cold Zone (LPN HOT_ZONE_LPNS ~ 끝)을 타겟
                lpn = (rand() % COLD_ZONE_LPNS) + HOT_ZONE_LPNS;
            }
            // -------------------------------

            if ((rand() % 100) < WRITE_PERCENTAGE) {
                if (!ftl.write(lpn)) {
                    std::cout << "\n--- Simulation " << sim + 1 << " stopped due to a fatal error at operation " << i + 1 << " ---" << std::endl;
                    break;
                }
            } else {
                // 읽기 작업은 단순화를 위해 전체 영역에서 랜덤하게 수행
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
    std::cout << "--- WAF Distribution Statistics (Hot/Cold) ---" << std::endl;

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