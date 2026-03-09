
#ifndef METRIC_TENSOR_HPP

#define METRIC_TENSOR_HPP


#include <array> // using over vector as we will be creating thousands of temporary tensors

class metric_tensor {

private:
    // 4x4 matrix stored flat for memory contiguity and cache locality.
    // Indices: 0=t, 1=x, 2=y, 3=z
    std::array<double, 16> G{};
    // The metric encodes the geometry of a
    // space by expressing deviations from Pythagoras's theorem
public:
    // Default constructor
    metric_tensor() {
        G.fill(0.0);
    }
};







#endif // METRIC_TENSOR_HPP



//For this to work MetricTensor needs to be dynamic
// Spacetime curvature is
//