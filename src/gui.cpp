#include "gui.hpp"
#include "physics.hpp"

#include "stb_image.h"
#include "stb_image_write.h"

float italic_factor = 1.0f / 3.5f;
float bold_factor = 1.0f;

Glyph_data& Font::at(uint32_t key) {
    if(glyph_map.find(key) != glyph_map.end()) return glyph_map[key];
    return empty_data;
}

enum bdf_region{BDF_NULL, BDF_HEADER, BDF_GLYPH};
bool bitmap = false;
void decode_font() {
    uint32_t line_height;

    std::string filepath = "res/other resources/alter_mono.afont";

    struct Glyph_ {
        bool visible;
        uint8_t stride;
        std::array<uint8_t, 2> size;
        std::array<uint16_t, 2> pos_tex;
        std::array<uint8_t, 2> pos_line;
    };
    
    std::unordered_map<uint8_t, Glyph_> map;
    std::vector<uint8_t> keys;

    std::vector<uint8_t> bytes = get_bytes_from_file(filepath);
    uint32_t pos = 0;

    auto read = [&](void* ptr, uint32_t num_bytes) {
        memcpy(ptr, &bytes[pos], num_bytes);
        pos += num_bytes;
    };

    read(&line_height, 1);

    uint16_t invisible_glyphs;
    read(&invisible_glyphs, 2);

    for(int i = 0; i < invisible_glyphs; ++i) {
        uint8_t id;
        Glyph_ data;
        data.visible = false;

        read(&id, 1);
        read(&data.stride, 1);

        map.insert({id, data});
        keys.push_back(id);
    }

    uint16_t visible_glyphs;
    read(&visible_glyphs, 2);

    for(int i = 0; i < visible_glyphs; ++i) {
        uint8_t id;
        Glyph_ data;
        data.visible = true;

        read(&id, 1);
        read(&data.stride, 1);
        read(&data.size[0], 1);
        read(&data.size[1], 1);
        read(&data.pos_tex[0], 2);
        read(&data.pos_tex[1], 2);
        read(&data.pos_line[0], 1);
        read(&data.pos_line[1], 1);

        map.insert({id, data});
        keys.push_back(id);
    }

    ivec2 tex_size;
    int num_channels;
    uint8_t* data = stbi_load("res/textures/text_mono.png", &tex_size.x, &tex_size.y, &num_channels, 0);
    
    std::vector<uint8_t> texture(data, data + (tex_size.x * tex_size.y * num_channels));

    std::vector<bool> bitmap(tex_size.x * tex_size.y);
    for(int i = 0; i < tex_size.x * tex_size.y; ++i) {
        uint32_t ii = i * 4 + 3;
        bitmap[i] = (texture[ii] != 0x0);
    }

    std::string file = R"(STARTFONT 0.0
SIZE 13 72 72
FONTBOUNDINGBOX 5 11 0 0
STARTPROPERTIES 20
FAMILY_NAME "Spleen"
WEIGHT_NAME "Medium"
FONT_VERSION "2.2.0"
FOUNDRY "misc"
SLANT "R"
SETWIDTH_NAME "Normal"
PIXEL_SIZE 12
POINT_SIZE 120
RESOLUTION_X 72
RESOLUTION_Y 72
SPACING "C"
AVERAGE_WIDTH 60
CHARSET_REGISTRY "ISO10646"
CHARSET_ENCODING "1"
MIN_SPACE 6
FONT_DESCENT 2
FONT_ASCENT 9
DEFAULT_CHAR 32
ENDPROPERTIES
CHARS 103
)";

    /*
    STARTCHAR char0
    ENCODING 0
    SWIDTH 500 0
    DWIDTH 6 0
    BBX 6 12 0 -2
    BITMAP
    00
    00
    D8
    88
    00
    88
    88
    00
    88
    D8
    00
    00
    ENDCHAR
    */
    
    for(uint8_t key : keys) {
        Glyph_ g = map[key];

        file += "STARTCHAR\n";
        file += "ENCODING " + to_base(int32_t(key), 10) + "\n";
        file += "SWIDTH 500 0\n";
        file += "DWIDTH " + to_base(int32_t(g.stride), 10) + " 0\n";


        if(!g.visible) {
            file += "BBX 0 0 0 0\n";
            file += "BITMAP\n";
        } else {
            ivec2 glyph_origin = {g.pos_tex[0], g.pos_tex[1]};
            ivec2 glyph_size = {g.size[0], g.size[1]};

            ivec4 bounding_box = ivec4(0x7FFFFFFF, 0x7FFFFFFF, -0x7FFFFFFF, -0x7FFFFFFF);
            for(int y = glyph_origin.y; y < glyph_origin.y + glyph_size.y; ++y) {
                for(int x = 0; x < glyph_size.x; ++x) {
                    ivec2 pos = {x + glyph_origin.x, y};
                    uint32_t index = pos.y * tex_size.x + pos.x;

                    if(bitmap[index]) {
                        bounding_box.x = min(bounding_box.x, pos.x);
                        bounding_box.y = min(bounding_box.y, pos.y);
                        bounding_box.z = max(bounding_box.z, pos.x);
                        bounding_box.w = max(bounding_box.w, pos.y);
                    }
                }
            }

            ivec2 offset = bounding_box.xy() - glyph_origin;
            ivec2 new_size = bounding_box.zw() - bounding_box.xy() + ivec2(1, 1);
            glyph_size = new_size;
            glyph_origin += offset;

            file += "BBX " + to_base(int32_t(glyph_size.x), 10) + " " + to_base(int32_t(glyph_size.y), 10) + " " + to_base(int32_t(offset.x + int(g.pos_line[0])), 10) + " " + to_base(int32_t(offset.y + int(g.pos_line[1])) - 2, 10) + "\n";
            file += "BITMAP\n";

            if(!(new_size.x < 0 || new_size.y < 0)) {
                for(int y = glyph_origin.y + glyph_size.y - 1; y >= glyph_origin.y; --y) {
                    std::vector<uint8_t> bits;
                    for(int x = 0; x < glyph_size.x; ++x) {
                        if(x % 8 == 0) bits.push_back(0x00);

                        ivec2 pos = {x + glyph_origin.x, y};
                        uint32_t index = pos.y * tex_size.x + pos.x;

                        if(bitmap[index]) {
                            bits[bits.size() - 1] |= (0x1 << (7 - x % 8));
                        }
                    }

                    for(uint8_t bit : bits) {
                        std::stringstream ss;
                        ss << std::uppercase << std::hex << std::setfill('0') << std::setw(2) << uint32_t(bit);

                        std::string s;
                        ss >> s;

                        file += s;
                    }
                    
                    file += "\n";
                }
            }
        }
        file += "ENDCHAR\n";
    }

    file += "ENDFONT";

    std::ofstream f("res\\other resources\\infinity.bdf", std::ios::trunc);

    f << file;

    f.close();
}

void Font::init(std::string filepath) {
    /*
    std::vector<uint8_t> bytes = get_bytes_from_file(filepath);
    uint32_t pos = 0;

    auto read = [&](void* ptr, uint32_t num_bytes) {
        memcpy(ptr, &bytes[pos], num_bytes);
        pos += num_bytes;
    };

    read(&line_height, 1);

    uint16_t invisible_glyphs;
    read(&invisible_glyphs, 2);

    for(int i = 0; i < invisible_glyphs; ++i) {
        uint8_t id;
        Glyph_data data;
        data.visible = false;

        read(&id, 1);
        read(&data.stride, 1);

        glyph_map.insert({id, data});
    }

    uint16_t visible_glyphs;
    read(&visible_glyphs, 2);

    for(int i = 0; i < visible_glyphs; ++i) {
        uint8_t id;
        Glyph_data data;
        data.visible = true;

        read(&id, 1);
        read(&data.stride, 1);
        read(&data.size[0], 1);
        read(&data.size[1], 1);
        read(&data.pos_tex[0], 2);
        read(&data.pos_tex[1], 2);
        read(&data.pos_line[0], 1);
        read(&data.pos_line[1], 1);

        glyph_map.insert({id, data});
    }

    // missing placeholder
    Glyph_data data;
    data.visible = true;
    read(&data.stride, 1);
    read(&data.size[0], 1);
    read(&data.size[1], 1);
    read(&data.pos_tex[0], 2);
    read(&data.pos_tex[1], 2);
    read(&data.pos_line[0], 1);
    read(&data.pos_line[1], 1);

    empty_data = data;
    */

    std::string text = get_text_from_file(filepath);

    /*
    STARTCHAR uni0001
    ENCODING 1
    SWIDTH 500 0
    DWIDTH 6 0
    BBX 5 6 1 1
    BITMAP
    F8
    88
    88
    88
    88
    F8
    ENDCHAR
    */

    ivec2 size;
    ivec2 offset;
    int line_ascent = 0;
    int line_descent = 0;

    std::stringstream ss(text);

    bdf_region region = BDF_NULL;

    uint32_t encoding;
    Glyph_data glyph;
    std::vector<uint32_t> keys;

    while(true) {
        std::string line;
        std::getline(ss, line);

        std::stringstream ss_line(line);
        std::string name;
        ss_line >> name;

        if(region == BDF_HEADER) {
            if(name == "FONTBOUNDINGBOX") {
                int xsize, ysize, xoffset, yoffset;
                ss_line >> xsize >> ysize >> xoffset >> yoffset;

                size = {xsize, ysize};
                offset = {xoffset, yoffset};
            } else if(name == "FONT_ASCENT") {
                ss_line >> line_ascent;
            } else if(name == "FONT_DESCENT") {
                ss_line >> line_descent;
            }
        } else if(region == BDF_GLYPH) {
            bool collect_bits = bitmap;

            if(name == "ENCODING") {
                int number;
                ss_line >> number;

                encoding = number;
            } else if(name == "DWIDTH") {
                int x, y;
                ss_line >> x >> y;

                glyph.advance = x;
            } else if(name == "BBX") {
                int xsize, ysize, xoffset, yoffset;
                ss_line >> xsize >> ysize >> xoffset >> yoffset;

                glyph.size = {xsize, ysize};
                glyph.offset = {xoffset, yoffset};
            } else if(name == "BITMAP") {
                bitmap = true;
                glyph.bitmap.clear();
            } else if(name == "ENDCHAR") {
                region = BDF_NULL;
                bitmap = false;
                collect_bits = false;

                keys.push_back(encoding);
                glyph_map.emplace(encoding, glyph);
            }

            if(collect_bits) {
                for(int i = 0; i < name.size() / 2; ++i) {
                    std::string bit(name.begin() + i * 2, name.begin() + (i + 1) * 2);

                    uint8_t b = from_base(bit, 16);

                    glyph.bitmap.push_back(b);
                }
            }
        }

        if(name == "STARTFONT") region = BDF_HEADER;
        else if(name == "ENDFONT") break;
        else if(name == "STARTCHAR") region = BDF_GLYPH;
    }

    std::vector<bool> pixels;
    ivec2 tex_size;
    uint32_t width = 1024;
    uint32_t num_per_row = width / size.x;
    uint32_t height = ceil(float(glyph_map.size()) / num_per_row) * size.y;
    tex_size = ivec2(width, height);

    pixels.resize(tex_size.x * tex_size.y, 0);

    uint32_t pos = 0;
    for(uint32_t i : keys) {
        Glyph_data& glyph = glyph_map[i];
        glyph.offset.y += line_descent;

        ivec2 origin = ivec2(pos % num_per_row, pos / num_per_row) * size;
        origin = origin + (glyph.offset - ivec2(0, line_descent) - offset);

        glyph.pos_tex = origin;

        uint32_t byte_row = ceil(float(glyph.size.x) / 8);

        bool visible = false;

        for(int y = 0; y < glyph.size.y; ++y) {
            for(int x = 0; x < byte_row; ++x) {
                int index = y * byte_row + x;
                uint8_t byte = glyph.bitmap[index];

                if(byte != 0x0) visible = true;

                for(int xx = x * 8; xx < min((x + 1) * 8, glyph.size.x); ++xx) {
                    int bit = 7 - (xx - x * 8);

                    bool b = (byte >> bit) & 0x1;

                    ivec2 bit_pos = origin + ivec2(xx, glyph.size.y - 1 - y);
                    int bit_index = bit_pos.y * tex_size.x + bit_pos.x;

                    pixels[bit_index] = b;
                }
            }
        }

        glyph.visible = visible;

        ++pos;
    }

    std::vector<uint8_t> texture(tex_size.x * tex_size.y * 4, 0x0);

    for(int i = 0; i < tex_size.x * tex_size.y; ++i) {
        if(pixels[i]) {
            texture[i * 4] = 0xFF;
            texture[i * 4 + 1] = 0xFF;
            texture[i * 4 + 2] = 0xFF;
            texture[i * 4 + 3] = 0xFF;
        }
    }

    core.textures.emplace("text_texture", std::make_shared<Texture>(Texture(texture.data(), ivec3(tex_size, 1), GL_TEXTURE_2D, {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE})));
    
    line_height = line_ascent + line_descent;

    //stbi_flip_vertically_on_write(true);
    //std::string filename = "output/screenshot" + to_base(int64_t(get_absolute_time() * 10), 10, true) + ".png";
    //stbi_write_png(filename.c_str(), tex_size.x, tex_size.y, 4, texture.data(), 4 * tex_size.x);
}

Font::Font(std::string filepath) {
    init(filepath);
}

std::vector<UI_vertex> create_char(Glyph_data& glyph) {
    std::vector<UI_vertex> ret;

    UI_vertex a = {vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec4(1.0f)};
    UI_vertex b = {vec2(1.0f, 0.0f), vec2(1.0f, 0.0f), vec4(1.0f)};
    UI_vertex c = {vec2(0.0f, 1.0f), vec2(0.0f, 1.0f), vec4(1.0f)};
    UI_vertex d = {vec2(1.0f, 1.0f), vec2(1.0f, 1.0f), vec4(1.0f)};

    ret.push_back(a);
    ret.push_back(b);
    ret.push_back(d);
    ret.push_back(a);
    ret.push_back(d);
    ret.push_back(c);

    for(UI_vertex& v : ret) {
        v.pos = v.pos * vec2(glyph.size) + vec2(glyph.offset);
        v.tex_pos = vec2(glyph.pos_tex) + v.tex_pos * vec2(glyph.size);
    }

    return ret;
}

std::vector<UI_vertex> mesh_text(Font& f, std::string str, uint32_t text_size, uint32_t width, ivec2 select_range, ALIGNMENT alignment, bool show_debug) {
    float italic_factor = 1.0f / 3.5f;
    float bold_factor = 1.0f;

    std::u32string text = convert_string(str);

    std::vector<int> text_line_indices;
    std::vector<int> text_line_origins;

    uint32_t line_start_index = 0;
    uint32_t word_start_index = 0;

    bool accept_index = false;

    int num_escape_seq = 0;
    int i = 0;

    std::vector<UI_vertex> ret;
    vec2 pos = vec2(0.0f);
    vec4 range = vec4(FLT_MAX, FLT_MAX, -FLT_MAX, -FLT_MAX);

    vec4 color = vec4(1.0f);
    bool bold = false;
    bool italic = false;
    bool hex = false;

    uint32_t num_lines = 0;

    std::vector<UI_vertex> word_ret;
    vec2 word_pos = vec2(0.0f);
    
    std::vector<UI_vertex> line_ret;

    auto insert_line = [&]() {
        int line_width = pos.x;
        int offset;

        if(alignment == ALIGNMENT_LEFT) offset = 0.0f;
        else if(alignment == ALIGNMENT_CENTER) offset = round(float(int(width) - line_width) / 2);
        else if(alignment == ALIGNMENT_RIGHT) offset = int(width) - line_width;

        for(UI_vertex& v : line_ret) {
            v.pos.x += offset;
        }
        
        ret.insert(ret.end(), line_ret.begin(), line_ret.end());
        
        line_ret.clear();

        pos.x = 0;
        pos.y -= f.line_height;
        ++num_lines;
        
        text_line_indices.push_back(line_start_index);
        text_line_origins.push_back(offset);
    };

    auto insert_word = [&]() {
        uint32_t end = pos.x + word_pos.x;

        if(end > width) {
            insert_line();
        }
        
        if(line_ret.size() == 0) line_start_index = word_start_index;

        // insert word
        for(UI_vertex& v : word_ret) {
            v.pos += pos;
        }
        
        line_ret.insert(line_ret.end(), word_ret.begin(), word_ret.end());

        word_ret.clear();
        
        pos.x += word_pos.x;
        word_pos = vec2(0.0f);  
    };

    auto insert_selection = [&](ivec2 pos, ivec2 size) {
        if(word_ret.size() == 0) {
            word_start_index = i;
        }

        UI_vertex a = {vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec4(1.0f)};
        UI_vertex b = {vec2(1.0f, 0.0f), vec2(1.0f, 0.0f), vec4(1.0f)};
        UI_vertex c = {vec2(0.0f, 1.0f), vec2(0.0f, 1.0f), vec4(1.0f)};
        UI_vertex d = {vec2(1.0f, 1.0f), vec2(1.0f, 1.0f), vec4(1.0f)};

        std::vector<UI_vertex> r = {a, b, d, a, d, c};
        for(UI_vertex& v : r) {
            v.pos = vec2(pos) + v.pos * vec2(size);
            v.tex_pos = vec2(1.0f, 63.0f);
            v.color = vec4(1.0f, 1.0f, 1.0f, 0.35f);
            v.data = 1;
        }
        word_ret.insert(word_ret.end(), r.begin(), r.end());
    };

    auto insert_char = [&](uint32_t codepoint) {
        Glyph_data& gd = f.at(codepoint);

        float stride = gd.advance;

        if(word_ret.size() == 0) {
            word_start_index = i;
        }

        if(!gd.visible) {
            UI_vertex v;
            v.pos = word_pos + vec2(stride, 0);
            v.data = 0xFFFFFFFF;
            word_ret.push_back(v);
            word_ret.push_back(v);
            word_ret.push_back(v);

            if(alignment == ALIGNMENT_LEFT) {
                if(i >= select_range.x && i < select_range.y) insert_selection(word_pos, {gd.advance, f.line_height});
                word_pos.x += stride;
                
                insert_word();
            } else if(alignment == ALIGNMENT_CENTER) {
                insert_word();

                if(i >= select_range.x && i < select_range.y) insert_selection(word_pos, {gd.advance, f.line_height});
                word_pos.x += stride;

                insert_word();
            } else if(alignment == ALIGNMENT_RIGHT) {
                insert_word();

                if(i >= select_range.x && i < select_range.y) insert_selection(word_pos, {gd.advance, f.line_height});
                word_pos.x += stride;
            }
        } else {
            std::vector<UI_vertex> vs = create_char(gd);

            for(UI_vertex& v : vs) {
                v.pos += word_pos;
            }

            for(UI_vertex& v : vs) {
                if(italic) {
                    v.pos.x += float(v.pos.y - word_pos.y - f.line_height * 0.5f) * italic_factor;
                }

                v.color = color;
            }

            word_ret.insert(word_ret.end(), vs.begin(), vs.end());

            if(bold) {
                for(UI_vertex& v : vs) {
                    v.pos.x += bold_factor;
                }
                stride += bold_factor;
                
                word_ret.insert(word_ret.end(), vs.begin(), vs.end());
            }
            
            if(i >= select_range.x && i < select_range.y) insert_selection(word_pos, {gd.advance + ((bold) ? bold_factor : 0.0f), f.line_height});

            word_pos.x += stride;
        }
    };

    for(i = 0; i < text.size(); ++i) {
        uint32_t c = text[i];

        if(c == '\n') {
            insert_word();
            insert_line();
            word_start_index = i;
            line_start_index = i;
            
            if(i >= select_range.x && i < select_range.y && (text[i + 1] == '\n' || i == text.size() - 1)) {
                if(alignment == ALIGNMENT_LEFT) insert_selection(word_pos, {6, f.line_height});
                else if(alignment == ALIGNMENT_CENTER) insert_selection(word_pos - vec2(3, 0), {6, f.line_height});
                else if(alignment == ALIGNMENT_RIGHT) insert_selection(word_pos - vec2(6, 0), {6, f.line_height});
            }

            continue;
        } else {
            if(c == '\\') {
                if(i + 1 < text.size()) {
                    uint32_t next = text[i + 1];

                    if(next == 'c') {
                        if(i + 1 + 3 < text.size()) {
                            std::string s(text.begin() + (i + 2), text.begin() + (i + 5));

                            std::size_t i0 = integers_letters.find(s[0]);
                            std::size_t i1 = integers_letters.find(s[1]);
                            std::size_t i2 = integers_letters.find(s[2]);

                            if(i0 != std::string::npos && i1 != std::string::npos && i2 != std::string::npos) {
                                color = vec4(float(i0) / 15.0f, float(i1) / 15.0f, float(i2) / 15.0f, 1.0f);
                                
                                num_escape_seq += 5;
                                if(!show_debug) {    
                                    i += 4;
                                    continue;   
                                }
                            }
                        }
                    } else if(next == 'b') {
                        bold = true;

                        num_escape_seq += 2;
                        if(!show_debug) {
                            i += 1;
                            continue;
                        }
                    } else if(next == 'i') {
                        italic = true;
                        
                        num_escape_seq += 2;
                        if(!show_debug) {
                            i += 1;
                            continue;
                        }

                    } else if(next == 'r') {
                        bold = false;
                        italic = false;
                        
                        num_escape_seq += 2;
                        if(!show_debug) {
                            i += 1;
                            continue;
                        }
                    } else if(next == 'h') {
                        hex = !hex;
                        
                        num_escape_seq += 2;
                        if(!show_debug) {
                            i += 1;
                            continue;
                        }
                    }
                }
            } 
            
            if(show_debug) {
                if(num_escape_seq > 0) {
                    color.w = 0.5f;
                    --num_escape_seq;

                    if(c == 'A') c = '\x80';
                    else if(c == 'B') c = '\x81';
                    else if(c == 'C') c = '\x82';
                    else if(c == 'D') c = '\x83';
                    else if(c == 'E') c = '\x84';
                    else if(c == 'F') c = '\x85';
                } else color.w = 1.0f;
            }

            if(hex) {
                if(c == 'A') c = '\x80';
                else if(c == 'B') c = '\x81';
                else if(c == 'C') c = '\x82';
                else if(c == 'D') c = '\x83';
                else if(c == 'E') c = '\x84';
                else if(c == 'F') c = '\x85';
            }
            
            insert_char(c);
        }
    }

    insert_word();
    insert_line();

    float offset = (num_lines - 1) * f.line_height;
    
    for(UI_vertex& v : ret) {
        v.pos.y = v.pos.y + offset;
        v.pos *= float(text_size);
    }

    for(UI_vertex& v : ret) {
        range.x = min(range.x, v.pos.x);
        range.y = min(range.y, v.pos.y);
        range.z = max(range.z, v.pos.x);
        range.w = max(range.w, v.pos.y);
    }

    return ret;
}

//

void GUI_system::init() {
    //decode_font();
    fonts.emplace("default mono", Font("res/other resources/infinity.bdf"));

    Signature s = ecs.update_signature<Camera>();
    ecs.update_signature<Transform>(s);
    collectors.push_back(Collector(s));

    //

    Window_Widget::insert(vec2(512, 512), vec2(96, 96));
    Panel_Widget::insert();
    
    Column_Widget::insert(vec2(2.0f, 2.0f));

    position(PM_TOP_LEFT);

    //std::string str = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Duis odio nibh, vestibulum a posuere sed, egestas ac augue. Praesent quis commodo augue. Duis iaculis suscipit quam, at scelerisque turpis egestas non. Suspendisse venenatis lectus ut malesuada tempus. Suspendisse tincidunt, justo et maximus ultricies, erat sapien ultricies mi, non fermentum risus nisl nec neque. Suspendisse auctor tortor non mattis consectetur. Morbi a augue hendrerit, consequat erat molestie, elementum turpis. Vestibulum convallis eleifend urna. Aenean auctor, ex a dictum sodales, ipsum arcu blandit neque, vitae sodales sapien lorem volutpat lacus. Aliquam rhoncus nisl elit, id tincidunt mi mattis sed. Suspendisse in ornare eros. Duis vehicula ex enim, ut gravida nisi tempus in. Nulla finibus, enim nec placerat facilisis, ante nunc commodo nisl, eget rutrum dui ipsum in nunc. Proin molestie elit quis nisl sagittis tristique.\x80\x81\x82\x83\x84\x85";
    //

    std::function<void(std::string&)> cb0 = [](std::string& str) {
        static double prev_time = get_time();
        static uint32_t num_frames = 0;

        double current_time = get_time();
        ++num_frames;

        if(current_time - prev_time > 1.0) {
            double dt = current_time - prev_time;
            prev_time = current_time;

            float fps = float(num_frames) / dt;
            num_frames = 0;

            std::string s = to_base(fps, 10, 3);

            str = "\\b\\cF44FPS: \\r\\cFFF" + s;
        }
    };

    std::function<void(std::string&)> cb1 = [](std::string& str) {
        Physics_system& ps = ecs.get_system<Physics_system>();
        
        uint32_t num_objects = ps.collectors[0].entities.size();
        
        str = "\\b\\c4F4★Physics Objects: \\r\\cFFF" + to_base(int32_t(num_objects), 10);
    };

    Text_Widget::insert("", ALIGNMENT_LEFT, cb1);
    Text_Widget::insert("", ALIGNMENT_LEFT, cb0);
}

void GUI_system::step() {
    current_widget = widgets[current_widget]->parent;
}

void GUI_system::position(POSITION_MODE mode) {
    active_position = mode;
}

void GUI_system::make_dirty(uint64_t root) {
    std::vector<uint64_t> path;
    std::vector<uint64_t> child_ids;

    //

    path = {root};
    child_ids = {0};
    while(true) {
        if (path.size() == 0) break;

        auto& widget = widgets[path.back()];

        vec2 children_size = vec2(0.0f);

        if (widget->children.size() <= child_ids.back()) {
            widget->dirty = true;

            // go up
            path.pop_back();
            child_ids.pop_back();
        } else {
            path.push_back(widget->children[child_ids.back()]);

            ++child_ids.back();
            child_ids.push_back(0);
        }
    }
}

void GUI_system::do_layout() {
    auto func_measure = [this](std::unique_ptr<Widget>& widget) {
        vec2 children_size = vec2(0.0f);
        
        if (widget->layout_mode == LM_ROW) {
            for (int i = 0; i < widget->children.size(); ++i) {
                auto& wchild = widgets[widget->children[i]];

                children_size.x += wchild->size.x;
                if(i != widget->children.size() - 1) children_size.x += widget->buffer.x;

                children_size.y = max(children_size.y, wchild->size.y);
            }
        } else if (widget->layout_mode == LM_COLUMN) {
            for (int i = 0; i < widget->children.size(); ++i) {
                auto& wchild = widgets[widget->children[i]];

                children_size.y += wchild->size.y;
                if(i != widget->children.size() - 1) children_size.y += widget->buffer.y;

                children_size.x = max(children_size.x, wchild->size.x);
            }
        } else if (widget->layout_mode == LM_GRID) {
            Grid_Widget& w = *(Grid_Widget*)widget.get();

            uint32_t num_rows = ceil(float(w.children.size()) / w.columns); 

            std::vector<float> columns(w.columns, 0.0f);
            std::vector<float> rows(num_rows, 0.0f);

            uint32_t index = 0;
            for(uint64_t child : w.children) {
                uint32_t column = index % w.columns;
                uint32_t row = index / w.columns;
                
                auto& wchild = widgets[child];
                rows[row] = max(rows[row], wchild->size.y);
                columns[column] = max(columns[column], wchild->size.x);
                
                ++index;
            }

            vec2 size = vec2(0.0f);
            for(float f : columns) size.x += f;
            for(float f : rows) size.y += f;

            children_size = size;
        } else if(widget->layout_mode == LM_VOID) {

        }

        if(widget->size_mode == SM_SURROUND) {
            widget->min_width = children_size.x;
        } else if(widget->size_mode == SM_STATIC) {
            widget->min_width = widget->size.x;
        }

        widget->on_measure();
    };

    std::function<std::vector<std::pair<uint64_t, float>>(std::unique_ptr<Widget>& widget, float)> solve_x = [this](std::unique_ptr<Widget>& widget, float width) {
        float min_width = 0.0f;
        float max_width = width * 100.0f;
        max_width = max(max_width, 0.0f);

        if(get_width_x(max_width, widget->self, 1.0f) < width) min_width = max_width;

        //

        for(int i = 0; i < 16; ++i) {
            float w = (min_width + max_width) * 0.5f;

            float ww = get_width_x(w, widget->self, 1.0f);

            if(ww > width) max_width = w;
            else min_width = w;
        }
        
        std::vector<std::pair<uint64_t, float>> shadowed_children;
        assign_width_x(min_width, widget->self, 1.0f, shadowed_children);

        return shadowed_children;
    };

    std::function<void(std::unique_ptr<Widget>&, bool)> func_solve_x = [this, &func_solve_x, &solve_x](std::unique_ptr<Widget>& widget, bool b) {
        if(widget->size_mode != SM_STATIC && (!(widget->parent == NULL_WIDGET) && widgets[widget->parent]->flag) || b) {
            float width = widgets[widget->parent]->child_region.z - widgets[widget->parent]->child_region.x;

            std::vector<std::pair<uint64_t, float>> shadow = solve_x(widget, width);

            if(shadow.size()) {
                std::vector<std::pair<uint64_t, float>> front_buffer = shadow;
                std::vector<std::pair<uint64_t, float>> back_buffer;

                while(true) {
                    for(auto [key, w] : front_buffer) {
                        auto shadow = solve_x(widgets[key], w);

                        back_buffer.insert(back_buffer.end(), shadow.begin(), shadow.end());
                    }

                    front_buffer = std::move(back_buffer);
                    back_buffer.clear();

                    if(front_buffer.size() == 0) break;
                }
            }
        }
        if(widget->size_mode == SM_SURROUND) {

        } else if(widget->min_width <= widget->max_width) {
            //widget->size.x = widget->get_height(widget);
        }
    };

    
    std::function<std::vector<std::pair<uint64_t, float>>(std::unique_ptr<Widget>& widget, float)> solve_y = [this](std::unique_ptr<Widget>& widget, float width) {
        float min_width = 0.0f;
        float max_width = width * 100.0f;
        max_width = max(max_width, 0.0f);

        if(get_width_y(max_width, widget->self, 1.0f) < width) min_width = max_width;

        //

        for(int i = 0; i < 16; ++i) {
            float w = (min_width + max_width) * 0.5f;

            float ww = get_width_y(w, widget->self, 1.0f);

            if(ww > width) max_width = w;
            else min_width = w;
        }
        
        std::vector<std::pair<uint64_t, float>> shadowed_children;
        assign_width_y(min_width, widget->self, 1.0f, shadowed_children);

        return shadowed_children;
    };

    std::function<void(std::unique_ptr<Widget>&, bool)> func_solve_y = [this, &func_solve_y, &solve_y](std::unique_ptr<Widget>& widget, bool b) {
        if(widget->size_mode != SM_STATIC && (!(widget->parent == NULL_WIDGET) && widgets[widget->parent]->flag) || b) {
            float width = widgets[widget->parent]->child_region.w - widgets[widget->parent]->child_region.y;

            std::vector<std::pair<uint64_t, float>> shadow = solve_y(widget, width);

            if(shadow.size()) {
                std::vector<std::pair<uint64_t, float>> front_buffer = shadow;
                std::vector<std::pair<uint64_t, float>> back_buffer;

                while(true) {
                    for(auto [key, w] : front_buffer) {
                        auto shadow = solve_y(widgets[key], w);

                        back_buffer.insert(back_buffer.end(), shadow.begin(), shadow.end());
                    }

                    front_buffer = std::move(back_buffer);
                    back_buffer.clear();

                    if(front_buffer.size() == 0) break;
                }
            }
        }
        if(widget->size_mode == SM_SURROUND) {

        } else if(widget->min_width <= widget->max_width) {
            //widget->size.x = widget->get_height(widget);
        }
    };

    auto func_place = [this](std::unique_ptr<Widget>& widget) {
        if (widget->layout_mode == LM_ROW) {
            vec2 position = vec2(0.0f);
            float buffer = widget->sep.x;

            for(int i = 0; i < widget->children.size(); ++i) {
                auto& wchild = widgets[widget->children[i]];

                wchild->rel_position = position;
                position.x += wchild->size.x + buffer;

                if(wchild->position_mode == PM_TOP_LEFT || wchild->position_mode == PM_TOP_CENTER || wchild->position_mode == PM_TOP_RIGHT) {
                    wchild->rel_position.y = widget->size.y - wchild->size.y;
                } else if(wchild->position_mode == PM_CENTER_LEFT || wchild->position_mode == PM_CENTER || wchild->position_mode == PM_CENTER_RIGHT) {
                    wchild->rel_position.y = (widget->size.y - wchild->size.y) * 0.5f;
                } else if(wchild->position_mode == PM_BOTTOM_LEFT || wchild->position_mode == PM_BOTTOM_CENTER || wchild->position_mode == PM_BOTTOM_RIGHT) {
                    wchild->rel_position.y = 0.0f;
                }
            }
        } else if (widget->layout_mode == LM_COLUMN) {
            vec2 position = vec2(0.0f);
            float buffer = widget->sep.y;

            for(int i = 0; i < widget->children.size(); ++i) {
                auto& wchild = widgets[widget->children[i]];

                wchild->rel_position = position;
                position.y += wchild->size.y + buffer;

                if(wchild->position_mode == PM_TOP_LEFT || wchild->position_mode == PM_CENTER_LEFT || wchild->position_mode == PM_BOTTOM_LEFT) {
                    wchild->rel_position.x = 0.0f;
                } else if(wchild->position_mode == PM_TOP_CENTER || wchild->position_mode == PM_CENTER || wchild->position_mode == PM_BOTTOM_CENTER) {
                    wchild->rel_position.x = (widget->size.x - wchild->size.x) * 0.5f;
                } else if(wchild->position_mode == PM_TOP_RIGHT || wchild->position_mode == PM_CENTER_RIGHT || wchild->position_mode == PM_BOTTOM_RIGHT) {
                    wchild->rel_position.x = widget->size.x - wchild->size.x;
                }
            }
        } else if (widget->layout_mode == LM_GRID) {
            Grid_Widget& w = *(Grid_Widget*)widget.get();
            vec2 buffer = widget->sep;

            uint32_t num_rows = ceil(float(w.children.size()) / w.columns); 

            std::vector<float> columns(w.columns, 0.0f);
            std::vector<float> rows(num_rows, 0.0f);

            uint32_t index = 0;
            for(uint64_t child : w.children) {
                uint32_t column = index % w.columns;
                uint32_t row = index / w.columns;
                
                auto& wchild = widgets[child];
                rows[row] = max(rows[row], wchild->size.y);
                columns[column] = max(columns[column], wchild->size.x);
                
                ++index;
            }

            index = 0;
            for(uint64_t child : w.children) {
                uint32_t column = index % w.columns;
                uint32_t row = index / w.columns;

                vec2 origin = vec2(0.0f);

                for(int i = 0; i < column; ++i) {
                    origin.x += columns[i] + buffer.x;
                }
                
                for(int i = 0; i < row; ++i) {
                    origin.y += rows[i] + buffer.y;
                }

                vec4 box_range = vec4(origin, columns[column], rows[row]);
                
                auto& wchild = widgets[child];

                if(wchild->position_mode == PM_TOP_LEFT || wchild->position_mode == PM_CENTER_LEFT || wchild->position_mode == PM_BOTTOM_LEFT) {
                    wchild->rel_position.x = box_range.x;
                } else if(wchild->position_mode == PM_TOP_CENTER || wchild->position_mode == PM_CENTER || wchild->position_mode == PM_BOTTOM_CENTER) {
                    wchild->rel_position.x = box_range.x + (box_range.z - wchild->size.x) * 0.5f;
                } else if(wchild->position_mode == PM_TOP_RIGHT || wchild->position_mode == PM_CENTER_RIGHT || wchild->position_mode == PM_BOTTOM_RIGHT) {
                    wchild->rel_position.x = box_range.x + box_range.z - wchild->size.x;
                }

                if(wchild->position_mode == PM_TOP_LEFT || wchild->position_mode == PM_TOP_CENTER || wchild->position_mode == PM_TOP_RIGHT) {
                    wchild->rel_position.y = box_range.y + box_range.w - wchild->size.y;
                } else if(wchild->position_mode == PM_CENTER_LEFT || wchild->position_mode == PM_CENTER || wchild->position_mode == PM_CENTER_RIGHT) {
                    wchild->rel_position.y = box_range.y + (box_range.w - wchild->size.y) * 0.5f;
                } else if(wchild->position_mode == PM_BOTTOM_LEFT || wchild->position_mode == PM_BOTTOM_CENTER || wchild->position_mode == PM_BOTTOM_RIGHT) {
                    wchild->rel_position.y = box_range.y;
                }

                ++index;
            }
        } else if(widget->layout_mode == LM_VOID) {
            vec2 position = vec2(0.0f);
            float buffer = 0.0f;

            for(int i = 0; i < widget->children.size(); ++i) {
                auto& wchild = widgets[widget->children[i]];

                wchild->rel_position = position;
                position.y += wchild->size.y + buffer;
                wchild->rel_position.y = 0.0f;
                
                /*
                if(wchild->position_mode == PM_TOP_LEFT || wchild->position_mode == PM_TOP_CENTER || wchild->position_mode == PM_TOP_RIGHT) {
                    std::cout << widget->size.y << " " << wchild->size.y << "\n";
                    wchild->rel_position.y = widget->size.y - wchild->size.y;
                } else if(wchild->position_mode == PM_CENTER_LEFT || wchild->position_mode == PM_CENTER || wchild->position_mode == PM_CENTER_RIGHT) {
                    wchild->rel_position.y = (widget->size.y - wchild->size.y) * 0.5f;
                } else if(wchild->position_mode == PM_BOTTOM_LEFT || wchild->position_mode == PM_BOTTOM_CENTER || wchild->position_mode == PM_BOTTOM_RIGHT) {
                    wchild->rel_position.y = 0.0f;
                }*/
            }
        }
    };

    std::vector<uint64_t> path;
    std::vector<uint64_t> child_ids;

    //

    path = {0};
    child_ids = {0};
    while(true) {
        if (path.size() == 0) break;

        auto& widget = widgets[path.back()];

        vec2 children_size = vec2(0.0f);

        if (widget->children.size() <= child_ids.back()) {
            func_measure(widget);

            // go up
            path.pop_back();
            child_ids.pop_back();
        } else {
            path.push_back(widget->children[child_ids.back()]);

            ++child_ids.back();
            child_ids.push_back(0);
        }
    }

    //
    path = {0};
    child_ids = {0};
    while(true) {
        if (path.size() == 0) break;

        auto& widget = widgets[path.back()];

        if(child_ids.back() == 0) {
            func_solve_x(widget, false);
            widget->on_solve_x();
        }
        
        vec2 children_size = vec2(0.0f);

        if (widget->children.size() <= child_ids.back()) {
            widget->get_y();
            
            // go up
            path.pop_back();
            child_ids.pop_back();
        } else {
            path.push_back(widget->children[child_ids.back()]);

            ++child_ids.back();
            child_ids.push_back(0);
        }
    }
    
    // solve y
    path = {0};
    child_ids = {0};
    while(true) {
        if (path.size() == 0) break;

        auto& widget = widgets[path.back()];

        if(child_ids.back() == 0) {
            func_solve_y(widget, false);
            widget->on_solve_y();
        }
        
        vec2 children_size = vec2(0.0f);

        if (widget->children.size() <= child_ids.back()) {
            if(widget->layout_mode == LM_ROW) {
                for(uint64_t child : widget->children) {
                    auto& widget_child = widgets[child];
                    children_size = vec2(children_size.x + widget_child->size.x, max(children_size.y, widget_child->size.y));
                }
                children_size.x += (widget->children.size() - 1) * widget->sep.x;
                
                widget->size = children_size;
            } else if(widget->layout_mode == LM_COLUMN) {
                for(uint64_t child : widget->children) {
                    auto& widget_child = widgets[child];
                    children_size = vec2(max(children_size.x, widget_child->size.x), children_size.y + widget_child->size.y);
                }
                children_size.y += (widget->children.size() - 1) * widget->sep.y;

                widget->size = children_size;
            } else if(widget->layout_mode == LM_GRID) {
                Grid_Widget& w = *(Grid_Widget*)widget.get();

                uint32_t num_rows = ceil(float(w.children.size()) / w.columns); 

                std::vector<float> columns(w.columns, 0.0f);
                std::vector<float> rows(num_rows, 0.0f);

                uint32_t index = 0;
                for(uint64_t child : w.children) {
                    uint32_t column = index % w.columns;
                    uint32_t row = index / w.columns;
                    
                    auto& wchild = widgets[child];
                    rows[row] = max(rows[row], wchild->size.y);
                    columns[column] = max(columns[column], wchild->size.x);
                    
                    ++index;
                }

                vec2 size = vec2(0.0f);
                for(float f : columns) size.x += f;
                for(float f : rows) size.y += f;
                size.x += (columns.size() - 1) * widget->sep.x;
                size.y += (rows.size() - 1) * widget->sep.y;

                w.size = size;
            }
            
            // go up
            path.pop_back();
            child_ids.pop_back();
        } else {
            path.push_back(widget->children[child_ids.back()]);

            ++child_ids.back();
            child_ids.push_back(0);
        }
    }

    //

    path = {0};
    child_ids = {0};
    while(true) {
        if (path.size() == 0) break;

        auto& widget = widgets[path.back()];

        vec2 children_size = vec2(0.0f);

        if(child_ids.back() == 0) widget->on_place();

        if (widget->children.size() <= child_ids.back()) {
            func_place(widget);
            
            if(widget->position_mode == PM_STATIC) widget->rel_position = widget->position;

            // go up
            path.pop_back();
            child_ids.pop_back();
        } else {
            path.push_back(widget->children[child_ids.back()]);

            ++child_ids.back();
            child_ids.push_back(0);
        }
    }

    //

    std::unordered_map<uint64_t, vec2> positions;

    for(auto& [k, widget] : widgets) {
        vec2 origin = vec2(0.0f);

        uint64_t kk = k;
        while(true) {
            if(kk == NULL_WIDGET) break;

            auto& ww = widgets[kk];

            if(kk != k) origin += ww->child_offset.xy();
            origin += ww->rel_position;

            if(ww->position_mode == PM_STATIC) break;
            
            kk = ww->parent;
        }

        positions.emplace(k, origin);
    }

    for(auto& [k, origin] : positions) {
        if(widgets[k]->position != origin) widgets[k]->dirty = true;
        widgets[k]->position = origin;
        
        widgets[k]->child_region = vec4(origin, origin + widgets[k]->size);
    }
}

void GUI_system::call() {
    vertices.clear();
    cursor_mode = CURSOR_CLICK;

    do_layout();

    // mesh

    std::vector<uint64_t> path;
    std::vector<uint64_t> child_ids;

    path = {0};
    child_ids = {0};
    while(true) {
        if (path.size() == 0) break;

        auto& widget = widgets[path.back()];

        vec2 children_size = vec2(0.0f);

        if(child_ids.back() == 0) {
            widget->mesh();
            vertices.insert(vertices.end(), widget->vertices_before.begin(), widget->vertices_before.end());
        }

        if (widget->children.size() <= child_ids.back()) {
            vertices.insert(vertices.end(), widget->vertices_after.begin(), widget->vertices_after.end());

            // go up
            path.pop_back();
            child_ids.pop_back();
        } else {
            path.push_back(widget->children[child_ids.back()]);

            ++child_ids.back();
            child_ids.push_back(0);
        }
    }

    //

    path = {0};
    child_ids = {0};
    while(true) {
        if (path.size() == 0) break;

        auto& widget = widgets[path.back()];

        if (widget->children.size() <= child_ids.back()) {
            widget->handle_inputs();

            // go up
            path.pop_back();
            child_ids.pop_back();
        } else {
            path.push_back(widget->children[child_ids.back()]);

            ++child_ids.back();
            child_ids.push_back(0);
        }
    }
}

void GUI_system::propagate_down(uint64_t start, std::function<bool(std::unique_ptr<Widget>&)> func) {
    std::vector<uint64_t> path;
    std::vector<uint64_t> child_ids;

    path = {0};
    child_ids = {0};
    while(true) {
        if (path.size() == 0) break;

        auto& widget = widgets[path.back()];
        bool f = func(widget);

        if (widget->children.size() <= child_ids.back() || f) {

            // go up
            path.pop_back();
            child_ids.pop_back();
        } else {
            path.push_back(widget->children[child_ids.back()]);

            ++child_ids.back();
            child_ids.push_back(0);
        }
    }
}

void GUI_system::propagate_up(uint64_t start, std::function<void(std::unique_ptr<Widget>&)> func) {
    std::vector<uint64_t> path;
    std::vector<uint64_t> child_ids;

    path = {0};
    child_ids = {0};
    while(true) {
        if (path.size() == 0) break;

        auto& widget = widgets[path.back()];

        if (widget->children.size() <= child_ids.back()) {
            func(widget);

            // go up
            path.pop_back();
            child_ids.pop_back();
        } else {
            path.push_back(widget->children[child_ids.back()]);

            ++child_ids.back();
            child_ids.push_back(0);
        }
    }
}

//

bool includes(ivec2 point, ivec4 range) {
    return (point.x >= range.x && point.x < range.z && point.y >= range.y && point.y < range.w);
}

vec4 intersect_range(vec4 a, vec4 b) {
    return {max(a.x, b.x), max(a.y, b.y), min(a.z, b.z), min(a.w, b.w)};
}

vec4 GUI_system::get_range(uint64_t v) {
    uint64_t current = v;
    vec4 range = vec4(-FLT_MAX, -FLT_MAX, FLT_MAX, FLT_MAX);

    while(true) {
        uint64_t parent = widgets[current]->parent;

        if(parent == NULL_WIDGET) break;

        vec4 r = widgets[parent]->child_region;
        //r.x = floor(r.x);
        //r.y = floor(r.y);
        //r.z = ceil(r.z);
        //r.w = ceil(r.w);
        
        range = intersect_range(range, r);
        current = parent;
    }

    return range;
}

float GUI_system::get_width_x(float x, uint64_t id, float weight) {
    auto& widget = widgets[id];

    if(widget->layout_mode == LM_VOID) {
        if(widget->flag) return max(widget->min_width, min(x * widget->weight_width * weight, widget->max_width));
        else return widget->min_width + min(x * widget->weight_width * weight, widget->max_width - widget->min_width);
    } else if(widget->layout_mode == LM_ROW) {
        float w = 0.0f;
        for(uint64_t child : widget->children) {
            float ww = get_width_x(x, child, widget->weight_width * weight);
            w += ww;
            
            if(child != widget->children.back()) w += widget->sep.x;
        }

        return w;
    } else if(widget->layout_mode == LM_COLUMN) {
        float w = 0.0f;
        for(uint64_t child : widget->children) {
            float ww = get_width_x(x, child, widget->weight_width * weight);
            w  = max(w, ww);
        }

        return w;
    } else if(widget->layout_mode == LM_GRID) {
        Grid_Widget& g = *(Grid_Widget*)widget.get();

        std::vector<float> column_widths(g.columns, 0.0f);

        uint32_t index = 0;
        for(uint64_t child : widget->children) {
            uint32_t column = index % g.columns;
            float ww = get_width_x(x, child, widget->weight_width * weight);
            column_widths[column] = max(column_widths[column], ww);

            ++index;
        }

        float w = 0;
        int i = 0;
        for(float f : column_widths) {
            w += f;

            if(i != g.columns - 1) w += widget->sep.x;

            ++i;
        }

        return w;
    }
}

float GUI_system::assign_width_x(float x, uint64_t id, float weight, std::vector<std::pair<uint64_t, float>>& shadowed_children) {
    auto& widget = widgets[id];

    if(widget->layout_mode == LM_VOID) {
        float size;
        if(widget->flag) size = max(widget->min_width, min(x * widget->weight_width * weight, widget->max_width));
        else size = widget->min_width + min(x * widget->weight_width * weight, widget->max_width - widget->min_width);

        if(size != widget->size.x) widget->dirty = true;
        widget->size.x = size;

        widget->on_transform();
        
        return widget->size.x;
    } else if(widget->layout_mode == LM_ROW) {
        float w = 0.0f;
        std::vector<std::pair<uint64_t, float>> sc;
        for(uint64_t child : widget->children) {
            std::vector<std::pair<uint64_t, float>> sc2;
            float ww = assign_width_x(x, child, widget->weight_width * weight, sc2);
            sc.insert(sc.end(), sc2.begin(), sc2.end());

            w += ww;

            if(child != widget->children.back()) w += widget->sep.x;
        }

        widget->size.x = w;

        shadowed_children = sc;
        return w;
    } else if(widget->layout_mode == LM_COLUMN) {
        std::vector<float> widths;
        std::vector<std::vector<std::pair<uint64_t, float>>> sc2;

        float w = 0.0f;
        for(uint64_t child : widget->children) {
            std::vector<std::pair<uint64_t, float>> sc;
            float ww = assign_width_x(x, child, widget->weight_width * weight, sc);
            widths.push_back(ww);
            sc2.push_back(sc);
            w = max(w, ww);
        }

        std::vector<std::pair<uint64_t, float>> sc;
        for(int i = 0; i < widget->children.size(); ++i) {
            if(widths[i] < w) sc.push_back({widget->children[i], w});
            else sc.insert(sc.end(), sc2[i].begin(), sc2[i].end()); 
        }

        widget->size.x = w;

        shadowed_children = sc;
        return w;
    } else if(widget->layout_mode == LM_GRID) {
        Grid_Widget& g = *(Grid_Widget*)widget.get();

        std::vector<float> column_widths(g.columns, 0.0f);

        std::vector<std::vector<float>> widths(g.columns);
        std::vector<std::vector<std::vector<std::pair<uint64_t, float>>>> sc_columns(g.columns);

        uint32_t index = 0;
        for(uint64_t child : widget->children) {
            uint32_t column = index % g.columns;
            std::vector<std::pair<uint64_t, float>> sc;
            float ww = assign_width_x(x, child, widget->weight_width * weight, sc);

            sc_columns[column].push_back(sc);
            widths[column].push_back(ww);

            column_widths[column] = max(column_widths[column], ww);

            ++index;
        }

        std::vector<std::pair<uint64_t, float>> sc;

        index = 0;
        for(uint64_t child : widget->children) {
            uint32_t column = index % g.columns;
            uint32_t row = index / g.columns;

            float ww = widths[column][row];
            if(ww < column_widths[column]) sc.push_back({child, column_widths[column]});
            else sc.insert(sc.end(), sc_columns[column][row].begin(), sc_columns[column][row].end());

            ++index;
        }


        float w = 0;
        int i = 0;
        for(float f : column_widths) {
            w += f;
            
            if(i != g.columns - 1) w += widget->sep.x;
            ++i;
        }
        
        widget->size.x = w;

        shadowed_children = sc;
        return w;
    }
}


float GUI_system::get_width_y(float x, uint64_t id, float weight) {
    auto& widget = widgets[id];

    if(widget->layout_mode == LM_VOID) {
        if(widget->flag) return max(widget->min_height, min(x * widget->weight_height * weight, widget->max_height));
        return widget->min_height + min(x * widget->weight_height * weight, widget->max_height - widget->min_height);
    } else if(widget->layout_mode == LM_ROW) {
        float w = 0.0f;
        for(uint64_t child : widget->children) {
            float ww = get_width_y(x, child, widget->weight_height * weight);
            w  = max(w, ww);
        }

        return w;
    } else if(widget->layout_mode == LM_COLUMN) {
        float w = 0.0f;
        for(uint64_t child : widget->children) {
            float ww = get_width_y(x, child, widget->weight_height * weight);
            w += ww;
            
            if(child != widget->children.back()) w += widget->sep.y;
        }

        return w;
    } else if(widget->layout_mode == LM_GRID) {
        Grid_Widget& g = *(Grid_Widget*)widget.get();

        uint32_t num_rows = g.children.size() / g.columns;

        std::vector<float> row_widths(num_rows, 0.0f);

        uint32_t index = 0;
        for(uint64_t child : widget->children) {
            uint32_t row = index / g.columns;
            float ww = get_width_y(x, child, widget->weight_height * weight);
            row_widths[row] = max(row_widths[row], ww);

            ++index;
        }

        float w = 0;
        uint32_t i = 0;
        for(float f : row_widths) {
            w += f;

            if(i != num_rows - 1) w += widget->sep.y;
            
            ++i;
        }

        return w;
    }
}

float GUI_system::assign_width_y(float x, uint64_t id, float weight, std::vector<std::pair<uint64_t, float>>& shadowed_children) {
    auto& widget = widgets[id];

    if(widget->layout_mode == LM_VOID) {
        float size;
        if(widget->flag) size = max(widget->min_height, min(x * widget->weight_height * weight, widget->max_height));
        else size = widget->min_height + min(x * widget->weight_height * weight, widget->max_height - widget->min_height);

        if(size != widget->size.y) widget->dirty = true;
        widget->size.y = size;
        
        widget->on_transform();
        
        return widget->size.y;
    } else if(widget->layout_mode == LM_ROW) {
        std::vector<float> widths;
        std::vector<std::vector<std::pair<uint64_t, float>>> sc2;

        float w = 0.0f;
        for(uint64_t child : widget->children) {
            std::vector<std::pair<uint64_t, float>> sc;
            float ww = assign_width_y(x, child, widget->weight_height * weight, sc);
            widths.push_back(ww);
            sc2.push_back(sc);
            w = max(w, ww);
        }

        std::vector<std::pair<uint64_t, float>> sc;
        for(int i = 0; i < widget->children.size(); ++i) {
            if(widths[i] < w) sc.push_back({widget->children[i], w});
            else sc.insert(sc.end(), sc2[i].begin(), sc2[i].end()); 
        }

        widget->size.y = w;

        shadowed_children = sc;
        return w;
    } else if(widget->layout_mode == LM_COLUMN) {
        float w = 0.0f;
        std::vector<std::pair<uint64_t, float>> sc;
        for(uint64_t child : widget->children) {
            std::vector<std::pair<uint64_t, float>> sc2;
            float ww = assign_width_y(x, child, widget->weight_height * weight, sc2);
            sc.insert(sc.end(), sc2.begin(), sc2.end());

            if(child != widget->children.back()) w += widget->sep.y;

            w += ww;
        }

        widget->size.y = w;

        shadowed_children = sc;
        return w;
    } else if(widget->layout_mode == LM_GRID) {
        Grid_Widget& g = *(Grid_Widget*)widget.get();

        uint32_t num_rows = g.children.size() / g.columns;

        std::vector<float> row_widths(num_rows, 0.0f);

        std::vector<std::vector<float>> widths(num_rows);
        std::vector<std::vector<std::vector<std::pair<uint64_t, float>>>> sc_rows(num_rows);

        uint32_t index = 0;
        for(uint64_t child : widget->children) {
            uint32_t row = index / g.columns;
            std::vector<std::pair<uint64_t, float>> sc;
            float ww = assign_width_y(x, child, widget->weight_height * weight, sc);

            sc_rows[row].push_back(sc);
            widths[row].push_back(ww);

            row_widths[row] = max(row_widths[row], ww);

            ++index;
        }

        std::vector<std::pair<uint64_t, float>> sc;

        index = 0;
        for(uint64_t child : widget->children) {
            uint32_t column = index % g.columns;
            uint32_t row = index / g.columns;

            float ww = widths[row][column];
            if(ww < row_widths[row]) sc.push_back({child, row_widths[row]});
            else sc.insert(sc.end(), sc_rows[row][column].begin(), sc_rows[row][column].end());

            ++index;
        }


        float w = 0;
        int i = 0;
        for(float f : row_widths) {
            w += f;
            
            if(i != num_rows - 1) w += widget->sep.y;
            ++i;
        }
        
        widget->size.y = w;

        shadowed_children = sc;
        return w;
    }
}


// window

void Window_Widget::insert(vec2 size, vec2 position) {
    GUI_system& gui_system = ecs.get_system<GUI_system>();

    Window_Widget widget;
    widget.size = size;
    widget.position = position;
    widget.flag = true;
    
    widget.size_mode = SM_STATIC;
    widget.layout_mode = LM_VOID;
    widget.position_mode = PM_STATIC;

    gui_system.insert_widget(widget, true);
}

void Window_Widget::handle_inputs() {
    bool close_window = false;

    GUI_system& gui_system = ecs.get_system<GUI_system>();

    float header = 15;
    float text_scale = 1;
    std::string label = "WINDOW";
    bool scrollbar = false;
    float shadow_width = 6;
    float buffer = 2;

    //

    /*
    bool scrollbar = false;
    int scrollbar_height = 0;

    vec2 scrollbar_rr = ws.scrollbar_r;
    if(ws.scrollbar_r.x == -1 && ws.scrollbar_r.y == -1) scrollbar_rr = vec2(0, size.y);

    ivec4 scrollbar_range;

    int y_size = (get_position().y - widget_sep) - (position.y - header);
    int height = abs(y_size - ws.scroll_pos);

    ws.scroll_pos = clamp(ws.scroll_pos, 0, max(0, height - int(size.y)));

    if(height > size.y && !(ws.panels.size() > 1)) {
        scrollbar = true;

        scrollbar_height = size.y * (float(size.y) / height);
        float ratio = 1.0f - ws.scroll_pos / (height - size.y);
        int32_t r = size.y - scrollbar_height;
        float rrr = ratio;

        scrollbar_range = {position + vec2(size.x - ws.scrollbar_width, -size.y - header + r * rrr), position + vec2(size.x, -size.y - header + scrollbar_height + r * rrr)};
    }
    */
    
    ivec4 buffer_range = ivec4(-buffer, -buffer, buffer, buffer);

    ivec4 range_move = {position + vec2(0.0f, size.y), position + vec2(size.x, size.y + header)};
    range_move += buffer_range;

    buffer = 4.0f;
    buffer_range = ivec4(-buffer, -buffer, buffer, buffer);

    ivec4 range_left = {position + vec2(0, 0), position + vec2(0, size.y + header)};
    ivec4 range_right = {position + vec2(size.x, 0), position + vec2(size.x, size.y + header)};
    ivec4 range_top = {position + vec2(0, size.y + header), position + vec2(size.x, size.y + header)};
    ivec4 range_bottom = {position + vec2(0, 0), position + vec2(size.x, 0)};
    //ivec4 hover_range = {position + vec2(0, -header - size.y), position + vec2(size.x, -header)};

    ivec4 range_close = {position + vec2(size.x - header, size.y), position + vec2(size.x, size.y + header)};

    range_left += buffer_range;
    range_right += buffer_range;
    range_top += buffer_range;
    range_bottom += buffer_range;

    vec2 min_size = vec2(192, 192);

    auto resize_left = [&]() {
        position.x += core.cursor_delta.x;
        size.x -= core.cursor_delta.x;

        float delta_max = (core.cursor_pos.x) - position.x;
        position.x += delta_max;
        size.x -= delta_max;

        float delta_min = max(0.0f, min_size.x - size.x);
        size.x += delta_min;
        position.x -= delta_min;
    };

    auto resize_right = [&]() {
        size.x += core.cursor_delta.x;

        float delta_max = (position.x + size.x) - (core.cursor_pos.x);
        size.x -= delta_max;

        float delta_min = max(0.0f, min_size.x - size.x);
        size.x += delta_min;
    };

    auto resize_top = [&]() {
        float delta_min;
        size.y += core.cursor_delta.y;

        float delta_max = (position.y + size.y + header) - (core.cursor_pos.y);
        size.y -= delta_max;

        delta_min = max(0.0f, min_size.y - size.y);
        size.y += delta_min; 
    };

    auto resize_bottom = [&]() {
        float delta_min;
        position.y += core.cursor_delta.y;
        size.y -= core.cursor_delta.y;
        
        float delta_max = (core.cursor_pos.y) - position.y;
        position.y += delta_max;
        size.y -= delta_max;

        delta_min = max(0.0f, min_size.y - size.y);
        position.y -= delta_min;  
        size.y += delta_min;
    };

    if(gui_system.capture_id == self) {
        switch(gui_system.capture_operation) {
            case 0:
                position += core.cursor_delta;
                dirty = true;
                break;
            case 1:
                resize_left();
                gui_system.cursor_mode = CURSOR_RESIZE_L;
                dirty = true;

                break;
            case 2:
                resize_right();
                gui_system.cursor_mode = CURSOR_RESIZE_R;
                dirty = true;

                break;
            case 3:
                resize_bottom();
                gui_system.cursor_mode = CURSOR_RESIZE_B;
                dirty = true;

                break;
            case 4:
                resize_left();
                resize_bottom();
                gui_system.cursor_mode = CURSOR_RESIZE_BL;
                dirty = true;

                break;
            case 5:
                resize_right();
                resize_bottom();
                gui_system.cursor_mode = CURSOR_RESIZE_BR;
                dirty = true;

                break;
            case 6:
                resize_top();
                gui_system.cursor_mode = CURSOR_RESIZE_T;
                dirty = true;

                break;
            case 7:
                resize_left();
                resize_top();
                gui_system.cursor_mode = CURSOR_RESIZE_TL;
                dirty = true;

                break;
            case 8:
                resize_right();
                resize_top();
                gui_system.cursor_mode = CURSOR_RESIZE_TR;
                dirty = true;

                break;
        }
    }

    if(gui_system.capture_id == NULL_WIDGET) {
        uint32_t operation = 0xFFFFFFFF;

        bool left_cont = includes(core.cursor_pos, range_left);
        bool right_cont = includes(core.cursor_pos, range_right);
        bool top_cont = includes(core.cursor_pos, range_top);
        bool bottom_cont = includes(core.cursor_pos, range_bottom);
        
        if(left_cont && bottom_cont) {
            gui_system.cursor_mode = CURSOR_RESIZE_BL;
            operation = 4;
        } else if(right_cont && bottom_cont) {
            gui_system.cursor_mode = CURSOR_RESIZE_BR;
            operation = 5;
        } else if(left_cont && top_cont) {
            gui_system.cursor_mode = CURSOR_RESIZE_TL;
            operation = 7;
        } else if(right_cont && top_cont) {
            gui_system.cursor_mode = CURSOR_RESIZE_TR;
            operation = 8;
        } else if(left_cont) {
            gui_system.cursor_mode = CURSOR_RESIZE_L;
            operation = 1;
        } else if(right_cont) {
            gui_system.cursor_mode = CURSOR_RESIZE_R;
            operation = 2;
        } else if(bottom_cont) {
            gui_system.cursor_mode = CURSOR_RESIZE_B;
            operation = 3;
        } else if(top_cont) {
            gui_system.cursor_mode = CURSOR_RESIZE_T;
            operation = 6;
        }

        /*
        if(includes(core.cursor_pos, scrollbar_range)) {
            gui_system.cursor_mode = CURSOR_CLICK;
            operation = 11;
            capture_position = (core.cursor_pos.y - scrollbar_range.w) / size.y;
        }
        */
        
        if(core.pressed_buttons.contains(GLFW_MOUSE_BUTTON_LEFT)) {
            if(operation != 0xFFFFFFFF) {
                gui_system.capture_id = self;
                gui_system.capture_operation = operation;
                
                //make_priority(name);
            } else {
                if(includes(core.cursor_pos, range_move)) {
                    gui_system.capture_id = self;
                    gui_system.capture_operation = 0;
                }

                if(includes(core.cursor_pos, range_close)) {
                    close_window = true;
                }
            }
        }
    }

    if(gui_system.capture_id == self) {
        if(!core.key_map[GLFW_MOUSE_BUTTON_LEFT]) gui_system.capture_id = NULL_WIDGET;
    }

    child_region = vec4(position, position + size);

    /*
    if(includes(core.cursor_pos, hover_range) && capture_window == name) {
        if(scrollbar && capture_window == name) {
            ws.scroll_pos = clamp(ws.scroll_pos + core.scroll_delta * scroll_speed * -1.0f, 0.0f, (height - size.y));
        }

        if(core.pressed_buttons.contains(GLFW_MOUSE_BUTTON_LEFT)) {
            capture = 4;
            make_priority(name);
        }
    }
    */
}

void Window_Widget::mesh() {
    if(dirty) {
        GUI_system& gui_system = ecs.get_system<GUI_system>();

        float header = 18;
        float text_scale = 1;
        std::string label = "WINDOW";
        bool scrollbar = false;
        float shadow_width = 6;

        //

        vec4 header_range = vec4(position + vec2(0.0f, size.y), position + vec2(size.x, size.y + header));

        std::vector<UI_vertex> total_ret;

        UI_vertex a = {vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec4(1.0f)};
        UI_vertex b = {vec2(1.0f, 0.0f), vec2(1.0f, 0.0f), vec4(1.0f)};
        UI_vertex c = {vec2(0.0f, 1.0f), vec2(0.0f, 1.0f), vec4(1.0f)};
        UI_vertex d = {vec2(1.0f, 1.0f), vec2(1.0f, 1.0f), vec4(1.0f)};

        // panel
        std::vector<UI_vertex> ret = {a, b, d, a, d, c};
        for(UI_vertex& v : ret) {
            v.pos = position + vec2(0.0f, 0.0f) + v.pos * size;
            v.tex_pos = vec2(1.0f, 63.0f);
            v.color = vec4(0.25f, 0.25f, 0.25f, 1.0f);
            v.data = 1;
        }
        total_ret.insert(total_ret.end(), ret.begin(), ret.end());

        // header
        ret = {a, b, d, a, d, c};
        for(UI_vertex& v : ret) {
            v.pos = position + vec2(0.0, size.y) + v.pos * vec2(size.x, header);
            v.tex_pos = vec2(1.0f, 63.0f);
            v.color = vec4(1.0f, 0.35f, 0.35f, 1.0f);
            v.data = 1;
        }
        total_ret.insert(total_ret.end(), ret.begin(), ret.end());

        Font& f = gui_system.fonts["default mono"];
        // label
        ret = mesh_text(f, label, 1);

        for(UI_vertex& v : ret) {
            float s = floor(header * 0.5f - f.line_height * float(text_scale) * 0.5f);
            v.pos = position + v.pos * float(text_scale) + vec2(max(s, 3.0f), -s - f.line_height * float(text_scale) + size.y + header);
            v.range = header_range;
        }
        total_ret.insert(total_ret.end(), ret.begin(), ret.end());

        vec4 range = vec4(59, 9, 64, 14);

        // close button
        ret = {a, b, d, a, d, c};
        for(UI_vertex& v : ret) {
            float s = floor(header * 0.5f - 5.0f * float(text_scale) * 0.5f);
            vec2 icon_size = vec2(range.z - range.x, range.w - range.y);

            v.pos = position + v.pos * float(text_scale) * icon_size + vec2(size.x - s - 5.0f * float(text_scale), -s - 5.0f * float(text_scale) + size.y + header);
            v.tex_pos = v.tex_pos * icon_size + vec2(range.x, range.y);
            v.color = vec4(1.0f);
            v.data = 1;
            v.range = header_range;
        }
        total_ret.insert(total_ret.end(), ret.begin(), ret.end());
        
        // shadow
        float shadow_w = 0.25f;
        
        // left
        ret = {a, b, d, a, d, c};
        ret[0].color.w = 0.0f;
        ret[3].color.w = 0.0f;
        ret[5].color.w = 0.0f;
        range = {position + vec2(-shadow_width, 0.0), position + vec2(0.0, size.y + header)};
        for(UI_vertex& v : ret) {
            v.pos = range.xy() + v.pos * (range.zw() - range.xy());
            v.tex_pos = vec2(1.0f, 63.0f);
            v.color = vec4(0.0f, 0.0f, 0.0f, v.color.w * shadow_w);
            v.data = 1;
        }
        total_ret.insert(total_ret.end(), ret.begin(), ret.end());

        // top left
        ret = {a, b, c, b, d, c};
        ret[0].color.w = 0.0f;
        ret[2].color.w = 0.0f;
        ret[4].color.w = 0.0f;
        ret[5].color.w = 0.0f;
        range = {position + vec2(-shadow_width, size.y + header), position + vec2(0.0f, shadow_width + size.y + header)};
        for(UI_vertex& v : ret) {
            v.pos = range.xy() + v.pos * (range.zw() - range.xy());
            v.tex_pos = vec2(1.0f, 63.0f);
            v.color = vec4(0.0f, 0.0f, 0.0f, v.color.w * shadow_w);
            v.data = 1;
        }
        total_ret.insert(total_ret.end(), ret.begin(), ret.end());
        
        // bottom left
        ret = {a, b, d, a, d, c};
        ret[0].color.w = 0.0f;
        ret[1].color.w = 0.0f;
        ret[3].color.w = 0.0f;
        ret[5].color.w = 0.0f;
        range = {position + vec2(-shadow_width, -shadow_width), position + vec2(0.0f, 0.0f)};
        for(UI_vertex& v : ret) {
            v.pos = range.xy() + v.pos * (range.zw() - range.xy());
            v.tex_pos = vec2(1.0f, 63.0f);
            v.color = vec4(0.0f, 0.0f, 0.0f, v.color.w * shadow_w);
            v.data = 1;
        }
        total_ret.insert(total_ret.end(), ret.begin(), ret.end());

        // right
        ret = {a, b, d, a, d, c};
        ret[1].color.w = 0.0f;
        ret[2].color.w = 0.0f;
        ret[4].color.w = 0.0f;
        range = {position + vec2(size.x, 0.0f), position + vec2(size.x + shadow_width, size.y + header)};
        for(UI_vertex& v : ret) {
            v.pos = range.xy() + v.pos * (range.zw() - range.xy());
            v.tex_pos = vec2(1.0f, 63.0f);
            v.color = vec4(0.0f, 0.0f, 0.0f, v.color.w * shadow_w);
            v.data = 1;
        }
        total_ret.insert(total_ret.end(), ret.begin(), ret.end());

        // top right
        ret = {a, b, d, a, d, c};
        ret[1].color.w = 0.0f;
        ret[2].color.w = 0.0f;
        ret[4].color.w = 0.0f;
        ret[5].color.w = 0.0f;
        range = {position + vec2(size.x, size.y + header), position + vec2(size.x + shadow_width, shadow_width + size.y + header)};
        for(UI_vertex& v : ret) {
            v.pos = range.xy() + v.pos * (range.zw() - range.xy());
            v.tex_pos = vec2(1.0f, 63.0f);
            v.color = vec4(0.0f, 0.0f, 0.0f, v.color.w * shadow_w);
            v.data = 1;
        }
        total_ret.insert(total_ret.end(), ret.begin(), ret.end());
        

        //bottom right
        ret = {a, b, c, b, d, c};
        ret[0].color.w = 0.0f;
        ret[1].color.w = 0.0f;
        ret[3].color.w = 0.0f;
        ret[4].color.w = 0.0f;
        range = {position + vec2(size.x, -shadow_width), position + vec2(size.x + shadow_width, 0.0f)};
        for(UI_vertex& v : ret) {
            v.pos = range.xy() + v.pos * (range.zw() - range.xy());
            v.tex_pos = vec2(1.0f, 63.0f);
            v.color = vec4(0.0f, 0.0f, 0.0f, v.color.w * shadow_w);
            v.data = 1;
        }
        total_ret.insert(total_ret.end(), ret.begin(), ret.end());

        // top
        ret = {a, b, d, a, d, c};
        ret[2].color.w = 0.0f;
        ret[4].color.w = 0.0f;
        ret[5].color.w = 0.0f;
        range = {position + vec2(0.0f, size.y + header), position + vec2(size.x, shadow_width + size.y + header)};
        for(UI_vertex& v : ret) {
            v.pos = range.xy() + v.pos * (range.zw() - range.xy());
            v.tex_pos = vec2(1.0f, 63.0f);
            v.color = vec4(0.0f, 0.0f, 0.0f, v.color.w * shadow_w);
            v.data = 1;
        }
        total_ret.insert(total_ret.end(), ret.begin(), ret.end());

        // bottom
        ret = {a, b, d, a, d, c};
        ret[0].color.w = 0.0f;
        ret[1].color.w = 0.0f;
        ret[3].color.w = 0.0f;
        range = {position + vec2(0.0f, -shadow_width), position + vec2(size.x, 0.0f)};
        for(UI_vertex& v : ret) {
            v.pos = range.xy() + v.pos * (range.zw() - range.xy());
            v.tex_pos = vec2(1.0f, 63.0f);
            v.color = vec4(0.0f, 0.0f, 0.0f, v.color.w * shadow_w);
            v.data = 1;
        }
        total_ret.insert(total_ret.end(), ret.begin(), ret.end());
        
        //

        vertices_before = total_ret;

        /*
        ws.current_panel = 0;
        ws.visited.clear();

        if(!ws.panels.size()) ws.panels.resize(1);
        for(auto& panel : ws.panels) panel.activated = false;

        if(window_state[active_window].clear_selection) {
            window_state[active_window].select_position = -1;
            window_state[active_window].select_anchor = -1;
            window_state[active_window].select_widget = "";
        }

        window_state[active_window].clear_selection = false;
        if(core.pressed_buttons.contains(GLFW_MOUSE_BUTTON_LEFT)) window_state[active_window].clear_selection = true;
        */


        dirty = false;
    }
}

// panel 

void Panel_Widget::handle_inputs() {
    GUI_system& gui_system = ecs.get_system<GUI_system>();

    rel_position = vec2(0.0f);
    if(parent != NULL_ENTITY) {
        auto& parent_widget = gui_system.widgets[parent];

        if(parent_widget->layout_mode == LM_VOID) {
            vec2 new_size = vec2(parent_widget->child_region.z - parent_widget->child_region.x, parent_widget->child_region.w - parent_widget->child_region.y);

            if(new_size != size) dirty = true;

            size = new_size;
            child_region = parent_widget->child_region;
        } else if(parent_widget->layout_mode == LM_ROW) {
            float new_size = parent_widget->child_region.w - parent_widget->child_region.y;

            if(new_size != size.y) dirty = true;

            size.y = new_size;
            child_region = {position.x, position.y, size.x + position.x, size.y + position.y};
        } 
    }

    int scroll_speed = 60;
    
    float scrollbar_height;
    vec4 scrollbar_range;

    float min_scroll = -total_scrollable;
    float max_scroll = 0.0f;
    if(max_scroll > min_scroll) {
        if(core.scroll_delta && includes(core.cursor_pos, child_region)) {
            dirty = true;

            float scroll_speed = 60.0f;
            child_offset.y -= core.scroll_delta * scroll_speed;
            child_offset.y = clamp(child_offset.y, min_scroll, max_scroll);
        }
    }
}

void Panel_Widget::mesh() {
    if(dirty) {
        dirty = false;
        
        std::vector<UI_vertex> ret;
        std::vector<UI_vertex> total_ret;

        UI_vertex a = {vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec4(1.0f)};
        UI_vertex b = {vec2(1.0f, 0.0f), vec2(1.0f, 0.0f), vec4(1.0f)};
        UI_vertex c = {vec2(0.0f, 1.0f), vec2(0.0f, 1.0f), vec4(1.0f)};
        UI_vertex d = {vec2(1.0f, 1.0f), vec2(1.0f, 1.0f), vec4(1.0f)};

        bool scrollbar = false;
        float scrollbar_height;
        float scrollbar_pos;

        ret = {a, b, d, a, d, c};
        for(UI_vertex& v : ret) {
            v.pos = position + v.pos * size;
            v.tex_pos = vec2(1.0f, 63.0f);
            v.color = vec4(0.125f, 0.125f, 0.125f, 1.0f);
            v.data = 1;
        }
        total_ret.insert(total_ret.end(), ret.begin(), ret.end());
        vertices_before = total_ret;
        total_ret.clear();

        if(total_scrollable > 0.0f) {
            scrollbar = true;

            float visible_height = child_region.w - child_region.y;
            float total_height = total_scrollable + visible_height;

            scrollbar_height = visible_height / total_height;
            scrollbar_pos = (-child_offset.y) / total_height;

            scrollbar_height *= visible_height;
            scrollbar_pos *= visible_height;
        }
        
        if(scrollbar) {
            vec2 size = vec2(2.0f, scrollbar_height);
            vec2 pos = vec2(child_region.z - 2.0f, child_region.y + scrollbar_pos);

            ret = {a, b, d, a, d, c};
            for(UI_vertex& v : ret) {
                v.pos = pos + v.pos * size;
                v.tex_pos = vec2(1.0f, 63.0f);
                v.color = vec4(1.0f, 1.0f, 1.0f, 0.25f);
                v.data = 1;
            }
            total_ret.insert(total_ret.end(), ret.begin(), ret.end());
        }

        vertices_after = total_ret;
    }
}

void Panel_Widget::on_place() {
    GUI_system& gui_system = ecs.get_system<GUI_system>();

    float height = child_region.w - child_region.y;

    float new_total_scrollable = 0.0f;
    if(children.size()) new_total_scrollable = gui_system.widgets[children[0]]->size.y - height;

    if(new_total_scrollable != total_scrollable) {
        child_offset.y = child_offset.y + total_scrollable - new_total_scrollable;
        if(new_total_scrollable < 0.0f) child_offset.y = -new_total_scrollable;
        else if(child_offset.y > 0.0f) child_offset.y = 0.0f;
        total_scrollable = new_total_scrollable;
        dirty = true;
    }
}

void Panel_Widget::on_transform() {
    vec4 new_region = vec4(position, position + size);
    new_region.x = floor(new_region.x);
    new_region.y = floor(new_region.y);
    new_region.z = ceil(new_region.z);
    new_region.y = ceil(new_region.y);
    if(child_region != new_region) {
        GUI_system& gui_system = ecs.get_system<GUI_system>();
        gui_system.make_dirty(self);
    }
    child_region = new_region;
}

void Panel_Widget::insert() {
    GUI_system& gui_system = ecs.get_system<GUI_system>();

    Panel_Widget widget;
    widget.position_mode = PM_BOTTOM_LEFT;
    widget.layout_mode = LM_VOID;
    widget.size_mode = SM_FILL;

    widget.min_width = 0.0f;
    widget.max_width = FLT_MAX;
    widget.min_height = 0.0f;
    widget.max_height = FLT_MAX;

    widget.flag = true;

    widget.size = vec2(0.0f);
    widget.position = vec2(0.0f);

    gui_system.insert_widget(widget, true);
}

// debug

void Debug_Widget::mesh() {
    if(dirty) {
        GUI_system& gui_system = ecs.get_system<GUI_system>();

        auto& pw = gui_system.widgets[parent];

        std::vector<UI_vertex> total_ret;

        UI_vertex a = {vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec4(1.0f)};
        UI_vertex b = {vec2(1.0f, 0.0f), vec2(1.0f, 0.0f), vec4(1.0f)};
        UI_vertex c = {vec2(0.0f, 1.0f), vec2(0.0f, 1.0f), vec4(1.0f)};
        UI_vertex d = {vec2(1.0f, 1.0f), vec2(1.0f, 1.0f), vec4(1.0f)};

        vec4 range = gui_system.get_range(self);

        // panel
        std::vector<UI_vertex> ret = {a, b, d, a, d, c};
        for(UI_vertex& v : ret) {
            v.pos = position + vec2(0.0f, 0.0f) + v.pos * size;
            v.tex_pos = vec2(1.0f, 63.0f);
            v.color = vec4(color, 1.0f);
            v.data = 1;

            v.range = range;
        }
        total_ret.insert(total_ret.end(), ret.begin(), ret.end());

        vertices_before = total_ret;
        dirty = false;
    }
}

void Debug_Widget::insert(vec2 size, float max_width, vec3 color) {
    GUI_system& gui_system = ecs.get_system<GUI_system>();

    Debug_Widget widget;
    widget.size = size;
    widget.color = color;
    widget.position_mode = gui_system.active_position;

    widget.min_width = size.x;
    widget.max_width = max_width;
    widget.min_height = size.y;
    widget.max_height = size.y;

    if(widget.max_width > widget.min_width) widget.size_mode = SM_FILL;

    gui_system.insert_widget(widget);
}

// column

void Column_Widget::insert(vec2 border) {
    GUI_system& gui_system = ecs.get_system<GUI_system>();

    Column_Widget widget;
    widget.size_mode = SM_SURROUND;
    widget.layout_mode = LM_COLUMN;
    widget.position_mode = gui_system.active_position;
    widget.sep = border;

    gui_system.insert_widget(widget, true);
}

// row

void Row_Widget::insert(vec2 border) {
    GUI_system& gui_system = ecs.get_system<GUI_system>();

    Row_Widget widget;
    widget.size_mode = SM_SURROUND;
    widget.layout_mode = LM_ROW;
    widget.position_mode = gui_system.active_position;
    widget.sep = border;

    gui_system.insert_widget(widget, true);
}

// grid

void Grid_Widget::insert(uint32_t num_columns, vec2 border) {
    GUI_system& gui_system = ecs.get_system<GUI_system>();

    Grid_Widget widget;
    widget.size_mode = SM_SURROUND;
    widget.layout_mode = LM_GRID;
    widget.position_mode = gui_system.active_position;
    widget.columns = num_columns;
    widget.sep = border;
 
    gui_system.insert_widget(widget, true);
}

// text

void Text_Widget::handle_inputs() {
    std::string str = text;
    callback(str);

    set_str(str);
}

void Text_Widget::mesh() {
    if(dirty) {
        dirty = false;
        
        GUI_system& gui_system = ecs.get_system<GUI_system>();
        Font& f = gui_system.fonts["default mono"];

        auto vs = mesh_text(f, text, text_size, size.x, {-1, -1}, alignment, false);

        vec4 range = gui_system.get_range(self);

        for(UI_vertex& v : vs) {
            v.pos = position + v.pos;
            v.range = range;
        }

        vertices_before = vs;
    }
}

void Text_Widget::get_y() {
    if(text_width != size.x) {
        text_width = size.x;

        dirty = true;

        std::u32string str = convert_string(text);

        //

        std::vector<UI_vertex> ret;

        bool show_debug = false;

        GUI_system& gui_system = ecs.get_system<GUI_system>();
        Font& f = gui_system.fonts["default mono"];

        uint32_t line_start_index = 0;
        uint32_t word_start_index = 0;

        bool accept_index = false;

        int num_escape_seq = 0;
        int i = 0;

        vec2 pos = vec2(0.0f);
        vec4 range = vec4(FLT_MAX, FLT_MAX, -FLT_MAX, -FLT_MAX);

        vec4 color = vec4(1.0f);
        bool bold = false;
        bool italic = false;
        bool hex = false;

        uint32_t num_lines = 0;

        vec2 word_pos = vec2(0.0f);

        auto insert_line = [&]() {
            int line_width = pos.x;
            int offset;

            pos.x = 0;
            pos.y -= float(f.line_height) * text_size;
            ++num_lines;
        };

        auto insert_word = [&]() {
            uint32_t end = pos.x + word_pos.x;

            if(end > size.x) {
                insert_line();
            }

            pos.x += word_pos.x;
            word_pos = vec2(0.0f);
        };

        auto insert_char = [&](uint32_t c) {
            Glyph_data& gd = f.at(c);

            float stride = gd.advance;

            if(!gd.visible) {
                if(alignment == ALIGNMENT_LEFT) {
                    word_pos.x += stride * text_size;

                    insert_word();
                } else if(alignment == ALIGNMENT_CENTER) {
                    insert_word();

                    word_pos.x += stride * text_size;

                    insert_word();
                } else if(alignment == ALIGNMENT_RIGHT) {
                    insert_word();

                    word_pos.x += stride * text_size;
                }
            } else {
                if(bold) {
                    stride += bold_factor;
                }

                word_pos.x += stride * text_size;
            }
        };

        for(i = 0; i < str.size(); ++i) {
            uint32_t c = str[i];

            if(c == '\n') {
                insert_word();
                insert_line();
                word_start_index = i;
                line_start_index = i;

                continue;
            } else {
                if(c == '\\') {
                    if(i + 1 < str.size()) {
                        uint32_t next = str[i + 1];

                        if(next == 'c') {
                            if(i + 1 + 3 < str.size()) {
                                std::u32string s(str.begin() + (i + 2), str.begin() + (i + 5));

                                std::size_t i0 = integers_letters.find(s[0]);
                                std::size_t i1 = integers_letters.find(s[1]);
                                std::size_t i2 = integers_letters.find(s[2]);

                                if(i0 != std::string::npos && i1 != std::string::npos && i2 != std::string::npos) {
                                    color = vec4(float(i0) / 15.0f, float(i1) / 15.0f, float(i2) / 15.0f, 1.0f);

                                    num_escape_seq += 5;
                                    if(!show_debug) {
                                        i += 4;
                                        continue;
                                    }
                                }
                            }
                        } else if(next == 'b') {
                            bold = true;

                            num_escape_seq += 2;
                            if(!show_debug) {
                                i += 1;
                                continue;
                            }
                        } else if(next == 'i') {
                            italic = true;

                            num_escape_seq += 2;
                            if(!show_debug) {
                                i += 1;
                                continue;
                            }

                        } else if(next == 'r') {
                            bold = false;
                            italic = false;

                            num_escape_seq += 2;
                            if(!show_debug) {
                                i += 1;
                                continue;
                            }
                        } else if(next == 'h') {
                            hex = !hex;

                            num_escape_seq += 2;
                            if(!show_debug) {
                                i += 1;
                                continue;
                            }
                        }
                    }
                }

                if(show_debug) {
                    if(num_escape_seq > 0) {
                        color.w = 0.5f;
                        --num_escape_seq;

                        if(c == 'A') c = '\x80';
                        else if(c == 'B') c = '\x81';
                        else if(c == 'C') c = '\x82';
                        else if(c == 'D') c = '\x83';
                        else if(c == 'E') c = '\x84';
                        else if(c == 'F') c = '\x85';
                    } else color.w = 1.0f;
                }

                if(hex) {
                    if(c == 'A') c = '\x80';
                    else if(c == 'B') c = '\x81';
                    else if(c == 'C') c = '\x82';
                    else if(c == 'D') c = '\x83';
                    else if(c == 'E') c = '\x84';
                    else if(c == 'F') c = '\x85';
                }

                insert_char(c);
            }
        }

        insert_word();
        insert_line();

        size.y = num_lines * f.line_height * text_size;

        min_height = size.y;
        max_height = size.y;
    }
}

void Text_Widget::set_str(std::string str) {
    if(text != str) dirty = true;
    text = str;
}

void Text_Widget::insert(std::string str, ALIGNMENT alg, std::function<void(std::string&)> callback_) {
    GUI_system& gui_system = ecs.get_system<GUI_system>();
    
    Text_Widget widget;

    widget.text = str;
    widget.callback = callback_;
    widget.size = vec2(0.0f);
    widget.min_width = 0.0f;
    widget.max_width = FLT_MAX;
    widget.min_height = 0.0f;
    widget.max_height = 0.0f;

    widget.position = vec2(0.0f);
    widget.alignment = alg;
    
    widget.size_mode = SM_FILL;
    widget.layout_mode = LM_VOID;
    widget.position_mode = gui_system.active_position;

    gui_system.insert_widget(widget);
}

void Split_Widget::insert(LAYOUT_MODE layout, std::vector<Panel_Constraint> constraints_) {
    GUI_system& gui_system = ecs.get_system<GUI_system>();

    Split_Widget widget;
    widget.size_mode = SM_SURROUND;
    widget.layout_mode = layout;
    widget.position_mode = PM_BOTTOM_LEFT;
    widget.sep = vec2(2.0f);
    widget.constraints = constraints_;
    
    gui_system.insert_widget(widget, true);
}

void Split_Widget::handle_inputs() {
    GUI_system& gui_system = ecs.get_system<GUI_system>();
    
    float buffer = 4.0f;
    vec4 buffer_range = ivec4(-buffer, -buffer, buffer, buffer);

    for(int i = 0; i < children.size() - 1; ++i) {
        int i0 = i;
        int i1 = i + 1;

        auto& w0 = gui_system.widgets[children[i0]];
        auto& w1 = gui_system.widgets[children[i1]];

        if(layout_mode == LM_ROW) {
            vec4 range = vec4(w1->position.x, w1->position.y, w1->position.x, w1->position.y + w1->size.y);

            range += buffer_range;

            if(includes(core.cursor_pos, range)) {
                gui_system.cursor_mode = CURSOR_RESIZE_L;

                if(core.pressed_buttons.contains(GLFW_MOUSE_BUTTON_LEFT)) {
                    gui_system.capture_id = self;
                    gui_system.capture_operation = i;
                }
            }
        } else if(layout_mode == LM_COLUMN) {
            vec4 range = vec4(w1->position.x, w1->position.y, w1->position.x + w1->size.x, w1->position.y);

            range += buffer_range;

            if(includes(core.cursor_pos, range)) {
                gui_system.cursor_mode = CURSOR_RESIZE_B;
                
                if(core.pressed_buttons.contains(GLFW_MOUSE_BUTTON_LEFT)) {
                    gui_system.capture_id = self;
                    gui_system.capture_operation = i;
                }
            }
        }
    }

    if(gui_system.capture_id == self) {
        if(!core.key_map[GLFW_MOUSE_BUTTON_LEFT]) gui_system.capture_id = NULL_WIDGET;

        if(layout_mode == LM_ROW) {
            gui_system.cursor_mode = CURSOR_RESIZE_L;

            int i = gui_system.capture_operation;

            Panel_Constraint& c0 = constraints[i];
            Panel_Constraint& c1 = constraints[i + 1];

            auto& w0 = gui_system.widgets[children[i]];
            auto& w1 = gui_system.widgets[children[i + 1]];

            float s = size.x;
            float p = position.x;
            float c = core.cursor_pos.x;
            
            c = clamp(c, w0->position.x + w0->min_width, w1->position.x + w1->size.x - w1->min_width);

            float r = (c - w0->position.x);
            float new_ratio = r / s;

            float difference = c0.value - new_ratio;
            c0.value -= difference;
            c1.value += difference;
        } else if(layout_mode == LM_COLUMN) {
            gui_system.cursor_mode = CURSOR_RESIZE_B;

            int i = gui_system.capture_operation;

            Panel_Constraint& c0 = constraints[i];
            Panel_Constraint& c1 = constraints[i + 1];

            auto& w0 = gui_system.widgets[children[i]];
            auto& w1 = gui_system.widgets[children[i + 1]];

            float s = size.y;
            float p = position.y;
            float c = core.cursor_pos.y;
            
            c = clamp(c, w0->position.y + w0->min_height, w1->position.y + w1->size.y - w1->min_height);

            float r = (c - w0->position.y);
            float new_ratio = r / s;

            float difference = c0.value - new_ratio;
            c0.value -= difference;
            c1.value += difference;
        }
    }

    on_transform();

    // handle inputs
}

void Split_Widget::on_transform() {
    float mw = 20.0f;

    if(layout_mode == LM_ROW) {
        GUI_system& gui_system = ecs.get_system<GUI_system>();

        float total = 0.0f;

        for(Panel_Constraint& constraint : constraints) {
            if(constraint.fill) {
                total += constraint.value;
            }
        }
        for(Panel_Constraint& constraint : constraints) if(constraint.fill) constraint.value /= total;

        float total_area = size.x;

        float borders = (children.size() - 1) * sep.x;

        total_area -= borders;

        min_width = 0;
        min_height = 0;

        for(int i = 0; i < children.size(); ++i) {
            auto& child = gui_system.widgets[children[i]];
            Panel_Constraint& constraint = constraints[i];

            if(child->flag) {
                child->min_width = mw;
                child->min_height = mw;
            }
            constraint.value = max(constraint.value, child->min_width / size.x);

            child->weight_width = constraint.value;
            child->weight_height = 1.0f;
            
            min_width += child->min_width;
            min_height = max(min_height, child->min_height);
        }
    } else if(layout_mode == LM_COLUMN) {
        GUI_system& gui_system = ecs.get_system<GUI_system>();

        float total = 0.0f;

        for(Panel_Constraint& constraint : constraints) {
            if(constraint.fill) {
                total += constraint.value;
            }
        }
        for(Panel_Constraint& constraint : constraints) if(constraint.fill) constraint.value /= total;

        float total_area = size.y;

        float borders = (children.size() - 1) * sep.y;

        total_area -= borders;
        
        min_width = 0;
        min_height = 0;

        for(int i = 0; i < children.size(); ++i) {
            auto& child = gui_system.widgets[children[i]];
            Panel_Constraint& constraint = constraints[i];

            if(child->flag) {
                child->min_width = mw;
                child->min_height = mw;
            }

            constraint.value = max(constraint.value, child->min_height / size.y);
            
            child->weight_height = constraint.value;
            child->weight_width = 1.0f;
            
            min_height += child->min_height;
            min_width = max(min_width, child->min_width);
        }
    }
}

void Split_Widget::on_measure() {
    
}