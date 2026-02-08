#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <future>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

class Image {
public:
    int width, height, channels;
    std::vector<unsigned char> pixels;

    Image(const std::string& filename) {
        unsigned char* img_data = stbi_load(filename.c_str(), &width, &height, &channels, 0);
        if (!img_data) throw std::runtime_error("Erreur de chargement.");
        pixels.assign(img_data, img_data + (width * height * channels));
        stbi_image_free(img_data);
    }

    void save(const std::string& filename) {
        stbi_write_jpg(filename.c_str(), width, height, channels, pixels.data(), 100);
    }

    void toGrayscale() {
        for (int i = 0; i < pixels.size(); i += channels) {
            int gray = (pixels[i] + pixels[i+1] + pixels[i+2]) / 3;
            for(int c=0; c<std::min(channels, 3); ++c) pixels[i+c] = (unsigned char)gray;
        }
    }

    // Version parallélisée du Flou
    void applyBlurParallel() {
        std::vector<unsigned char> original = pixels;
        int num_threads = std::thread::hardware_concurrency();
        std::vector<std::future<void>> futures;

        auto blur_chunk = [&](int start_y, int end_y) {
            for (int y = start_y; y < end_y; ++y) {
                if (y == 0 || y >= height - 1) continue;
                for (int x = 1; x < width - 1; ++x) {
                    for (int c = 0; c < channels; ++c) {
                        int sum = 0;
                        for (int ky = -1; ky <= 1; ++ky)
                            for (int kx = -1; kx <= 1; ++kx)
                                sum += original[((y + ky) * width + (x + kx)) * channels + c];
                        pixels[(y * width + x) * channels + c] = sum / 9;
                    }
                }
            }
        };

        int chunk_size = height / num_threads;
        for (int i = 0; i < num_threads; ++i) {
            int start = i * chunk_size;
            int end = (i == num_threads - 1) ? height : (i + 1) * chunk_size;
            futures.push_back(std::async(std::launch::async, blur_chunk, start, end));
        }
        for (auto& f : futures) f.get();
    }

    void applySobel() {
        toGrayscale();
        std::vector<unsigned char> original = pixels;
        for (int y = 1; y < height - 1; ++y) {
            for (int x = 1; x < width - 1; ++x) {
                int gx = -original[((y-1)*width+(x-1))*channels] + original[((y-1)*width+(x+1))*channels]
                         -2*original[(y*width+(x-1))*channels]  + 2*original[(y*width+(x+1))*channels]
                         -original[((y+1)*width+(x-1))*channels] + original[((y+1)*width+(x+1))*channels];

                int gy = -original[((y-1)*width+(x-1))*channels] - 2*original[((y-1)*width+x)*channels] - original[((y-1)*width+(x+1))*channels]
                         +original[((y+1)*width+(x-1))*channels] + 2*original[((y+1)*width+x)*channels] + original[((y+1)*width+(x+1))*channels];

                int val = std::clamp((int)std::sqrt(gx*gx + gy*gy), 0, 255);
                for(int c=0; c<channels; ++c) pixels[(y*width+x) * channels + c] = (unsigned char)val;
            }
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cout << "Usage: " << argv[0] << " <input.jpg> <output.jpg> <filter>\n";
        std::cout << "Filtres: --gray, --blur, --sobel\n";
        return 1;
    }

    try {
        Image img(argv[1]);
        std::string filter = argv[3];

        if (filter == "--gray") img.toGrayscale();
        else if (filter == "--blur") img.applyBlurParallel();
        else if (filter == "--sobel") img.applySobel();
        else { std::cout << "Filtre inconnu.\n"; return 1; }

        img.save(argv[2]);
        std::cout << "Succes !\n";
    } catch (const std::exception& e) {
        std::cerr << "Erreur: " << e.what() << "\n";
    }
    return 0;
}