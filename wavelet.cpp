#include "wavelet.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <cmath>

#if defined(__AVX2__) || defined(_M_AMD64) || defined(_M_X64)
#include <immintrin.h>
#endif

/**
 * AVX2 Optimized Haar 1D Transform
 * 
 * Shuffle Logic (De-interleaving):
 * We start with 8 floats in a YMM register: [a0, b0, a1, b1, a2, b2, a3, b3]
 * We use _mm256_permutevar8x32_ps with indices [0, 2, 4, 6, 1, 3, 5, 7]
 * This separates the vector into two 128-bit halves:
 *   Low 128-bit:  [a0, a1, a2, a3] (Even pixels / A)
 *   High 128-bit: [b0, b1, b2, b3] (Odd pixels / B)
 * We then compute Avg = (A+B)*0.5 and Diff = (A-B) in parallel.
 */
#if defined(__AVX2__)
void haar1D_AVX2(const std::vector<float>& line, std::vector<float>& temp, int h) {
    int i = 0;
    __m256i mask = _mm256_setr_epi32(0, 2, 4, 6, 1, 3, 5, 7);
    __m256 half = _mm256_set1_ps(0.5f);

    for (; i <= h - 4; i += 4) {
        // Load 8 floats (4 pairs)
        __m256 v = _mm256_loadu_ps(&line[i * 2]);
        
        // De-interleave
        __m256 perm = _mm256_permutevar8x32_ps(v, mask);
        
        // Extract evens and odds
        __m128 evens = _mm256_extractf128_ps(perm, 0);
        __m128 odds  = _mm256_extractf128_ps(perm, 1);
        
        // Math
        __m128 avgs = _mm_mul_ps(_mm_add_ps(evens, odds), _mm_set1_ps(0.5f));
        __m128 diffs = _mm_sub_ps(evens, odds);
        
        // Store
        _mm_storeu_ps(&temp[i], avgs);
        _mm_storeu_ps(&temp[h + i], diffs);
    }
    
    // Tail handling
    for (; i < h; ++i) {
        float a = line[2 * i];
        float b = line[2 * i + 1];
        temp[i] = (a + b) / 2.0f;
        temp[h + i] = (a - b);
    }
}
#endif

void haar1D(std::vector<float>& line, int size) {
    if (size < 2) return;

    std::vector<float> temp(size);
    int h = size / 2;

#if defined(__AVX2__)
    haar1D_AVX2(line, temp, h);
#else
    for (int i = 0; i < h; ++i) {
        float a = line[2 * i];
        float b = line[2 * i + 1];
        temp[i] = (a + b) / 2.0f;
        temp[h + i] = (a - b);
    }
#endif

    // Copy back
    for (int i = 0; i < size; ++i) {
        line[i] = temp[i];
    }
}

void invHaar1D(std::vector<float>& line, int size) {
    if (size < 2) return;

    std::vector<float> temp(size);
    int h = size / 2;

    for (int i = 0; i < h; ++i) {
        float avg = line[i];
        float detail = line[h + i];

        // Reconstruction:
        // a = avg + detail / 2
        // b = avg - detail / 2
        temp[2 * i] = avg + detail / 2.0f;
        temp[2 * i + 1] = avg - detail / 2.0f;
    }

    for (int i = 0; i < size; ++i) {
        line[i] = temp[i];
    }
}

void transform2D(GrayImage& img) {
    // 1. Transform Rows
    for (int y = 0; y < img.height; ++y) {
        std::vector<float> row(img.width);
        for (int x = 0; x < img.width; ++x) {
            row[x] = img.data[y * img.width + x];
        }
        haar1D(row, img.width);
        for (int x = 0; x < img.width; ++x) {
            img.data[y * img.width + x] = row[x];
        }
    }

    // 2. Transform Columns
    for (int x = 0; x < img.width; ++x) {
        std::vector<float> col(img.height);
        for (int y = 0; y < img.height; ++y) {
            col[y] = img.data[y * img.width + x];
        }
        haar1D(col, img.height);
        for (int y = 0; y < img.height; ++y) {
            img.data[y * img.width + x] = col[y];
        }
    }
}

void inverseTransform2D(GrayImage& img) {
    // 1. Inverse Columns
    for (int x = 0; x < img.width; ++x) {
        std::vector<float> col(img.height);
        for (int y = 0; y < img.height; ++y) {
            col[y] = img.data[y * img.width + x];
        }
        invHaar1D(col, img.height);
        for (int y = 0; y < img.height; ++y) {
            img.data[y * img.width + x] = col[y];
        }
    }

    // 2. Inverse Rows
    for (int y = 0; y < img.height; ++y) {
        std::vector<float> row(img.width);
        for (int x = 0; x < img.width; ++x) {
            row[x] = img.data[y * img.width + x];
        }
        invHaar1D(row, img.width);
        for (int x = 0; x < img.width; ++x) {
            img.data[y * img.width + x] = row[x];
        }
    }
}

bool loadPGM(const std::string& path, GrayImage& img) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;

    std::string format;
    int maxVal;
    file >> format >> img.width >> img.height >> maxVal;
    
    // Skip single whitespace after maxVal
    file.ignore(1);

    if (format != "P5") return false;

    img.data.assign(img.width * img.height, 0.0f);
    std::vector<uint8_t> buffer(img.width * img.height);
    file.read(reinterpret_cast<char*>(buffer.data()), buffer.size());

    for (size_t i = 0; i < buffer.size(); ++i) {
        img.data[i] = static_cast<float>(buffer[i]);
    }
    return true;
}

bool savePGM(const std::string& path, const GrayImage& img) {
    std::ofstream file(path, std::ios::binary);
    if (!file) return false;

    file << "P5\n" << img.width << " " << img.height << "\n255\n";

    std::vector<uint8_t> buffer(img.width * img.height);
    for (size_t i = 0; i < img.data.size(); ++i) {
        float val = std::clamp(img.data[i], 0.0f, 255.0f);
        buffer[i] = static_cast<uint8_t>(val);
    }
    file.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
    return true;
}

void applySaliency(GrayImage& img, float radius) {
    float cx = img.width / 2.0f;
    float cy = img.height / 2.0f;

    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            float dx = x - cx;
            float dy = y - cy;
            float dist = std::sqrt(dx * dx + dy * dy);

            if (dist > radius) {
                img.data[y * img.width + x] = 0.0f;
            }
        }
    }
}

std::vector<uint8_t> quantize(const GrayImage& img, float scale) {
    std::vector<uint8_t> output;
    // Reserve 4 bytes per pixel
    output.reserve(img.data.size() * 4);

    for (float val : img.data) {
        // Upgrade to 32-bit integer to prevent overflow at high scales
        int32_t quantized = static_cast<int32_t>(std::round(val * scale));
        
        // Split into 4 bytes (Big-Endian)
        output.push_back(static_cast<uint8_t>((quantized >> 24) & 0xFF));
        output.push_back(static_cast<uint8_t>((quantized >> 16) & 0xFF));
        output.push_back(static_cast<uint8_t>((quantized >> 8) & 0xFF));
        output.push_back(static_cast<uint8_t>(quantized & 0xFF));
    }
    return output;
}

void dequantize(const std::vector<uint8_t>& data, GrayImage& img, float scale) {
    // We assume the image dimensions are already set in 'img'
    img.data.assign(img.width * img.height, 0.0f);
    
    for (size_t i = 0; i < img.data.size(); ++i) {
        if (4 * i + 3 >= data.size()) break;

        // Reassemble from 4 bytes
        uint8_t b3 = data[4 * i];
        uint8_t b2 = data[4 * i + 1];
        uint8_t b1 = data[4 * i + 2];
        uint8_t b0 = data[4 * i + 3];
        
        // Cast to uint32_t first to prevent sign extension issues during shift
        int32_t val = (static_cast<uint32_t>(b3) << 24) |
                      (static_cast<uint32_t>(b2) << 16) |
                      (static_cast<uint32_t>(b1) << 8) |
                      static_cast<uint32_t>(b0);
                      
        img.data[i] = static_cast<float>(val) / scale;
    }
}
