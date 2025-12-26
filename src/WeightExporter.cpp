#include "WeightExporter.h"
#include "HX711.h"
#include <aixlog/aixlog.hpp>

#define HX_READ_TIMEOUT_MS (100)

constexpr double WeightExporter::OFFSET;
constexpr double WeightExporter::CALIBRATION_FACTOR;

double WeightExporter::read_weight_grams() {
    HX711* hx1;
    HX711* hx2;
    HX711* hx3;
    HX711* hx4;
    try {
        hx1 = new HX711("/dev/gpiochip0", cells[0].dt, cells[0].sck);
        hx2 = new HX711("/dev/gpiochip0", cells[1].dt, cells[1].sck);
        hx3 = new HX711("/dev/gpiochip0", cells[2].dt, cells[2].sck);
        hx4 = new HX711("/dev/gpiochip0", cells[3].dt, cells[3].sck);
    } catch(std::exception& e) {
        LOG(WARNING) << "Error with opening a hx711 load cell: " << e.what() << std::endl;
        return 0;
    }

    // Read average 5 times per sensor, as in Python
    long raw1 = hx1->read_average_with_timeout(5, HX_READ_TIMEOUT_MS);
    long raw2 = hx2->read_average_with_timeout(5, HX_READ_TIMEOUT_MS);
    long raw3 = hx3->read_average_with_timeout(5, HX_READ_TIMEOUT_MS);
    long raw4 = hx4->read_average_with_timeout(5, HX_READ_TIMEOUT_MS);

    // ensure we clean up memory
    delete hx1;
    delete hx2;
    delete hx3;
    delete hx4;

    long raw_total = raw1 + raw2 + raw3 + raw4;

    // Apply offset and calibration factor to get weight in grams
    double weight = (raw_total - OFFSET) / CALIBRATION_FACTOR;
    
    // floor at 0
    if(weight < 0)
        weight = 0;

    return weight;
}