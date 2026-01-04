#pragma once

struct LoadCellPins {
    unsigned int dt;
    unsigned int sck;
};

class WeightExporter {
public:
    WeightExporter() {}

    static double read_weight_grams();

private:
    // Load cell pin locations
    static constexpr LoadCellPins cells[4] = {
        {5, 6},   // hx1
        {17, 27}, // hx2
        {23, 24}, // hx3
        {13, 26}  // hx4
    };

    // Calibration factors
    static constexpr double OFFSET = -922041.33;
    static constexpr double CALIBRATION_FACTOR = -1006.033;

};