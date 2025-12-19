#include "wavelet.h"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <cassert>

void printImage(const GrayImage& img, const std::string& label) {
    std::cout << "--- " << label << " ---" << std::endl;
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            std::cout << std::fixed << std::setprecision(1) << std::setw(6) << img.data[y * img.width + x] << " ";
        }
        std::cout << std::endl;
    }
}

int main() {
    const int N = 8;
    GrayImage img(N, N);

    // Fill with some pattern
    for (int y = 0; y < N; ++y) {
        for (int x = 0; x < N; ++x) {
            img.data[y * N + x] = static_cast<float>(y * 10 + x);
        }
    }

    GrayImage original = img;
    printImage(img, "Original Image");

    // Forward Transform
    transform2D(img);
    printImage(img, "Wavelet Coefficients");

    // Inverse Transform
    inverseTransform2D(img);
    printImage(img, "Reconstructed Image");

    // Verification
    bool match = true;
    for (int i = 0; i < N * N; ++i) {
        if (std::abs(img.data[i] - original.data[i]) > 0.001f) {
            match = false;
            break;
        }
    }

    if (match) {
        std::cout << "\nRESULT: Implementation SUCCESSFUL (Perfect Reconstruction)!" << std::endl;
    } else {
        std::cout << "\nRESULT: Implementation FAILED (Data Mismatch)!" << std::endl;
    }

    return 0;
}
