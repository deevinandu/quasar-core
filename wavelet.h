#ifndef WAVELET_H
#define WAVELET_H

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

#endif // WAVELET_H
