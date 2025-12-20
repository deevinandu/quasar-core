#include "wavelet.h"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <cassert>

void printImage(const GrayImage& img, const std::string& label) {
    std::cout << "--- " << label << " ---" << std::endl;
    for (int y = 0; y < std::min(8, img.height); ++y) {
        for (int x = 0; x < std::min(8, img.width); ++x) {
            std::cout << std::fixed << std::setprecision(4) << std::setw(8) << img.data[y * img.width + x] << " ";
        }
        std::cout << std::endl;
    }
}

int main() {
    const int N = 8;
    const float scale = 1000.0f;
    GrayImage img(N, N);

    // Fill with some pattern
    for (int y = 0; y < N; ++y) {
        for (int x = 0; x < N; ++x) {
            img.data[y * N + x] = static_cast<float>(y * 10.1234f + x * 0.5678f);
        }
    }

    GrayImage original = img;
    std::cout << "Testing 32-bit High Precision Quantization with scale: " << scale << std::endl;
    printImage(img, "Original Image (top 8x8)");

    // 1. Transform
    transform2D(img);

    // 2. Quantize/Dequantize bridge
    auto quantized = quantize(img, scale);
    std::cout << "Quantized size: " << quantized.size() << " bytes (4 per pixel)" << std::endl;
    
    GrayImage reconstructed(N, N);
    dequantize(quantized, reconstructed, scale);

    // 3. Inverse Transform
    inverseTransform2D(reconstructed);
    printImage(reconstructed, "Reconstructed Image (top 8x8)");

    // 4. Verification
    float maxError = 0.0f;
    for (int i = 0; i < N * N; ++i) {
        float error = std::abs(reconstructed.data[i] - original.data[i]);
        if (error > maxError) maxError = error;
    }

    std::cout << "Maximum Reconstruction Error: " << maxError << std::endl;
    
    if (maxError < 0.001f) {
        std::cout << "RESULT: 16-bit High Precision Quantization SUCCESSFUL!" << std::endl;
    } else {
        std::cout << "RESULT: FAILED (Error too high)" << std::endl;
    }

    return 0;
}
