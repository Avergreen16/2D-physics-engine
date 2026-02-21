#include "erosion.hpp"
#include "random.hpp"

vec3 lcolor(float f) {
    f = fract(f);
    f *= 6.0f;

    float ff = fract(f);
    if(f < 1.0f) {
        return vec3(1.0f, ff, 0.0f);
    } else if(f < 2.0f) {
        return vec3(1.0f - ff, 1.0f, 0.0f);
    } else if(f < 3.0f) {
        return vec3(0.0f, 1.0f, ff);
    } else if(f < 4.0f) {
        return vec3(0.0f, 1.0f - ff, 1.0f);
    } else if(f < 5.0f) {
        return vec3(ff, 0.0f, 1.0f);
    } else {
        return vec3(1.0f, 0.0f, 1.0f - ff);
    }
}

Erosion_system::Erosion_system() {
    size = ivec2(1024, 512);

    elevation.resize(size.x * size.y);

    for(int y = 0; y < size.y; ++y) {
        for(int x = 0; x < size.x; ++x) {
            int i = x + y * size.x;
            vec2 position = vec2(x, y);

            elevation[i] = Noise_gen::perlin_noise(vec3(position, 0.0f), 384.0f, 5, 0xB3) * 0.5f + 0.5f;
        }
    }

    update_texture();
}

void Erosion_system::call() {

}

void Erosion_system::update_texture() {
    std::vector<uint8_t> texture(size.x * size.y * 4);

    for(int y = 0; y < size.y; ++y) {
        for(int x = 0; x < size.x; ++x) {
            int i = x + y * size.x;

            float c = clamp(elevation[i], 0.0f, 1.0f);

            vec3 color = lcolor(c * 6.0f) * 0.5f + 0.35f;

            vec<4, uint8_t> bit_color = {color * 255.0f, 255};

            texture[i * 4] = bit_color[0];
            texture[i * 4 + 1] = bit_color[1];
            texture[i * 4 + 2] = bit_color[2];
            texture[i * 4 + 3] = bit_color[3];
        }
    }

    if(map_texture) {
        map_texture.reset();
    }

    map_texture = std::shared_ptr<Texture>(new Texture(texture.data(), ivec3(size, 1), GL_TEXTURE_2D, {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE}, 0));
}