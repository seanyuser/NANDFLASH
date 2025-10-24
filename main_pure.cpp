#include <iostream>
#include <cstdlib>
#include <ctime>
#include <vector>      // ✅ 결과를 저장하기 위해 추가
#include <numeric>     // ✅ 평균 계산(std::accumulate)을 위해 추가
#include <algorithm>   // ✅ 최소/최대값(min/max_element)을 위해 추가
#include <iomanip>     // ✅ 소수점 출력을(std::setprecision) 위해 추가
#include "FTL.h"

int main() {
    // ✅ 랜덤 시드는 프로그램 시작 시 '한 번만' 초기화합니다.
    srand(time(0));

    const int TOTAL_OPERATIONS = 50000;
    const int WRITE_PERCENTAGE = 80;
    const int NUM_SIMULATIONS = 1000; // ✅ 1,000번의 시뮬레이션을 실행

    // ✅ 1,000개의 Final WAF 값을 저장할 벡터
    std::vector<double> final_wafs; 

    std::cout << "Starting " << NUM_SIMULATIONS << " SSD simulations..." << std::endl;
    std::cout << "Total operations per simulation: " << TOTAL_OPERATIONS << std::endl;
    std::cout << "Number of logical pages: " << NUM_LOGICAL_PAGES << std::endl;
    std::cout << "Number of physical pages: " << NUM_BLOCKS * PAGES_PER_BLOCK << std::endl;
    std::cout << "----------------------------------------" << std::endl;

    // ✅ 1,000번 시뮬레이션을 실행하는 외부 루프
    for (int sim = 0; sim < NUM_SIMULATIONS; ++sim) {
        
        // ✅ 매 시뮬레이션마다 FTL 객체를 새로 생성하여 상태를 초기화합니다.
        FTL ftl; 

        // 기존 50,000번 연산 루프
        for (int i = 0; i < TOTAL_OPERATIONS; ++i) {
            int lpn = rand() % NUM_LOGICAL_PAGES;

            if ((rand() % 100) < WRITE_PERCENTAGE) {
                if (!ftl.write(lpn)) {
                    std::cout << "\n--- Simulation " << sim + 1 << " stopped due to a fatal error at operation " << i + 1 << " ---" << std::endl;
                    break;
                }
            } else {
                ftl.read(lpn);
            }

            // ✅ 너무 많은 로그를 막기 위해 내부 진행 상황 출력은 주석 처리
            /*
            if ((i + 1) % (TOTAL_OPERATIONS / 10) == 0) {
                std::cout << i + 1 << " operations completed. Current WAF: " << ftl.getWAF() << std::endl;
            }
            */
        }

        // ✅ 이번 시뮬레이션의 최종 WAF 값을 벡터에 저장
        final_wafs.push_back(ftl.getWAF());

        // ✅ 100번의 시뮬레이션마다 진행 상황을 출력
        if ((sim + 1) % 100 == 0 || sim == NUM_SIMULATIONS - 1) {
            std::cout << "Simulation " << sim + 1 << "/" << NUM_SIMULATIONS << " completed." << std::endl;
        }
    }

    std::cout << "----------------------------------------" << std::endl;
    std::cout << "All " << NUM_SIMULATIONS << " simulations finished!" << std::endl;
    std::cout << "--- WAF Distribution Statistics ---" << std::endl;

    // ✅ 1,000개 WAF 데이터의 기본 통계 계산 및 출력
    if (!final_wafs.empty()) {
        double sum = std::accumulate(final_wafs.begin(), final_wafs.end(), 0.0);
        double average_waf = sum / final_wafs.size();
        double min_waf = *std::min_element(final_wafs.begin(), final_wafs.end());
        double max_waf = *std::max_element(final_wafs.begin(), final_wafs.end());

        std::cout << std::fixed << std::setprecision(5); // 소수점 5자리까지 고정
        std::cout << "Average WAF: " << average_waf << std::endl;
        std::cout << "Min WAF:     " << min_waf << std::endl;
        std::cout << "Max WAF:     " << max_waf << std::endl;
        std::cout << "\n(Data for " << final_wafs.size() << " successful runs collected)" << std::endl;
    }

    return 0;
}