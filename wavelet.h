#ifndef WAVELET_H
#define WAVELET_H
#include <string>   
#include <cstdint>

#include <vector>

struct GrayImage {
    int width;
    int height;
    std::vector<float> data;

    GrayImage(int w, int h) : width(w), height(h), data(w * h, 0.0f) {}
};

// Forward Haar 1D transform on a line of specific size
void haar1D(std::vector<float>& line, int size);

// Inverse Haar 1D transform
void invHaar1D(std::vector<float>& line, int size);

// Forward Haar 2D transform (Rows then Columns)
void transform2D(GrayImage& img);

// Inverse Haar 2D transform
void inverseTransform2D(GrayImage& img);

// PGM File Helpers
bool loadPGM(const std::string& path, GrayImage& img);
bool savePGM(const std::string& path, const GrayImage& img);

// Saliency filter: Nullifies coefficients outside a central radius
void applySaliency(GrayImage& img, float radius);

// Quantization: Bridges float coefficients to 16-bit bit-packed data
std::vector<uint8_t> quantize(const GrayImage& img, float scale);

// Dequantization: Reconstructs float coefficients from 16-bit data
void dequantize(const std::vector<uint8_t>& data, GrayImage& img, float scale);

#endif // WAVELET_H
