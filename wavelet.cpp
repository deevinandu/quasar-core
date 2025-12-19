#include "wavelet.h"
#include <algorithm>

void haar1D(std::vector<float>& line, int size) {
    if (size < 2) return;

    std::vector<float> temp(size);
    int h = size / 2;

    for (int i = 0; i < h; ++i) {
        float a = line[2 * i];
        float b = line[2 * i + 1];

        // Lifting scheme:
        // Avg = (a + b) / 2
        // Detail = a - b
        temp[i] = (a + b) / 2.0f;     // Averages in the first half
        temp[h + i] = (a - b);        // Details in the second half
    }

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
