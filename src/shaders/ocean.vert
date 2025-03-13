#version 460 core

const vec3 vertices[] = {
    vec3(-1, -1, -1),
    vec3(1, -1, -1),
    vec3(-1, 1, -1),
    vec3(1, 1, -1),
    vec3(-1, -1, 1),
    vec3(1, -1, 1),
    vec3(-1, 1, 1),
    vec3(1, 1, 1),
};

const int indices[] = {
    2, 0, 4,   2, 4, 6,
    1, 3, 7,   1, 7, 5,
    0, 1, 5,   0, 5, 4,
    3, 2, 6,   3, 6, 7,
    2, 3, 1,   2, 1, 0, 
    4, 5, 7,   4, 7, 6
};

const vec3 x_dir[] = {
    vertices[0] - vertices[2],
    vertices[3] - vertices[1],
    vertices[1] - vertices[0],
    vertices[2] - vertices[3],
    vertices[3] - vertices[2],
    vertices[5] - vertices[4]
};

const vec3 y_dir[] = {
    vertices[6] - vertices[2],
    vertices[5] - vertices[1],
    vertices[4] - vertices[0],
    vertices[7] - vertices[3],
    vertices[0] - vertices[2],
    vertices[6] - vertices[4]
};

const vec3 o_dir[] = {
    vertices[2],
    vertices[1],
    vertices[0],
    vertices[3],
    vertices[2],
    vertices[4]
};

layout(location = 0) uniform mat4 proj;
layout(location = 1) uniform mat4 view;
layout(location = 2) uniform mat4 model;
//layout(location = 3) uniform vec3 player_pos;
layout(location = 3) uniform vec3 axes;

int num_faces = 8;
int num_per_side = 4 * num_faces * num_faces;

uint total_num = num_faces * num_faces * 6 * 4;

void main() {
    vec3 vv;
    uint side = gl_VertexID / num_per_side;
    uint face_v = gl_VertexID - side * num_per_side;
    uint face = face_v / 4;

    uint face_x = face % num_faces;
    uint face_y = face / num_faces;
    uint vvv = gl_VertexID % 4;

    if(vvv == 0) {

    } else if(vvv == 1) {
        ++face_x;
    } else if(vvv == 2) {
        ++face_y;
    } else if(vvv == 3) {
        ++face_x;
        ++face_y;
    }

    vec3 x_d = x_dir[side];
    vec3 y_d = y_dir[side];
    vec3 o_d = o_dir[side];
    x_d /= num_faces;
    y_d /= num_faces;

    vv = o_d + x_d * face_x + y_d * face_y;

    vv = normalize(vv) * axes;

    gl_Position = vec4(vv, 1.0);
}