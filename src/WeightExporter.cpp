#include "WeightExporter.h"
#include "common.h"
#include <chrono>
#include <iomanip>
#include <aixlog/aixlog.hpp>
#include <thread>
#include <array>

#define HX_READ_TIMEOUT_MS (100)

constexpr double WeightExporter::OFFSET;
constexpr double WeightExporter::CALIBRATION_FACTOR;

constexpr LoadCellPins WeightExporter::cells[4];

double WeightExporter::read_weight_grams() {

    std::array<double, 4> reads;

    std::thread t1([&]{ reads[0] = get_hx_val(0); });
    std::thread t2([&]{ reads[1] = get_hx_val(1); });
    std::thread t3([&]{ reads[2] = get_hx_val(2); });
    std::thread t4([&]{ reads[3] = get_hx_val(3); });

    // Wait for all threads
    t1.join();
    t2.join();
    t3.join();
    t4.join();

    double val = reads[0] + reads[1] + reads[2] + reads[3];

    double off_val = (val - OFFSET) / CALIBRATION_FACTOR;
    if(off_val < 0) return 0.0f;
    else return off_val;
}

double WeightExporter::get_hx_val(int idx) {
    HX711::SimpleHX711 hx(cells[idx].dt, cells[idx].sck);
    hx.setUnit(HX711::Mass::Unit::G);
    HX711::Mass mass;
    try {
        mass = hx.weight(std::chrono::nanoseconds(500000000)); // 500ms timeout
    } catch(std::exception& e) {
        LOG(WARNING) << "Unable to obtain sample for load cell " << (idx + 1) << ", timeout reached." << std::endl;
        return 0.0f;
    }
    return mass.getValue();
}
