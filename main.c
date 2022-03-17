#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <GL/gl.h>
#include <cglm/cglm.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>

int W = 800, H = 800;
int R = 1 << 10;
vec3 eye = {0, 0, 0}, dir = {1, 0, 0}, up = {0, 0, 1};
float phi = 90, psi = 0;
float curt, prevt = 0;
float prevx = 0, prevy = 0;
float speed = 1, sens = 0.1;

GLFWwindow * initWindow() {
    glfwInit();
    glfwWindowHint(GLFW_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    GLFWwindow * window = glfwCreateWindow(W, H, "Raytracer", NULL, NULL);
    if (!window) {
        printf("Failed to initialize GLFW window\n");
    }

    glfwMakeContextCurrent(window);

    int gladok = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    if (!gladok) {
        printf("Failed to initialize GLAD\n");
    }
    return window;
}

const char * loadSource(const char * filename) {
    FILE * file = fopen(filename, "rb");
    fseek(file, 0, SEEK_END);
    size_t sz = ftell(file);
    fseek(file, 0, SEEK_SET);
    char * src = malloc(sz + 1);
    src[sz] = '\0';
    fread(src, 1, sz, file);
    fclose(file);
    return src;
}

uint loadShader(const char * file, int type) {
    const char * src = loadSource(file);

    uint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);
    free((void *)src);

    int ok;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        printf("Shader compilation failed\n");
        int sz;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &sz);
        char * infolog = malloc(sz);
        glGetShaderInfoLog(shader, sz, NULL, infolog);
        printf("%s\n", infolog);
        free(infolog);
    }
    return shader;
}

uint loadRenderProgram(const char * vertfile, const char * fragfile) {
    uint vertshader = loadShader(vertfile, GL_VERTEX_SHADER);
    uint fragshader = loadShader(fragfile, GL_FRAGMENT_SHADER);

    uint program = glCreateProgram();
    glAttachShader(program, vertshader);
    glAttachShader(program, fragshader);

    glLinkProgram(program);
    int ok;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        printf("Shader program linking failed\n");
        int sz;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &sz);
        char * infolog = malloc(sz);
        glGetProgramInfoLog(program, sz, NULL, infolog);
        printf("%s\n", infolog);
        free(infolog);
    }

    glDeleteShader(vertshader);
    glDeleteShader(fragshader);
    return program;
}

uint loadComputeProgram(const char * compfile) {
    uint compshader = loadShader(compfile, GL_COMPUTE_SHADER);

    uint program = glCreateProgram();
    glAttachShader(program, compshader);

    glLinkProgram(program);
    int ok;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        printf("Shader program linking failed\n");
        int sz;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &sz);
        char * infolog = malloc(sz);
        glGetProgramInfoLog(program, sz, NULL, infolog);
        printf("%s\n", infolog);
        free(infolog);
    }

    glDeleteShader(compshader);
    return program;
}

uint createScene() {
    int width = R, height = R, channels = 4;

    uint texture;
    glGenTextures(1, &texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
 
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
    glBindImageTexture(0, texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
    glBindTexture(GL_TEXTURE_2D, 0);
    return texture;
}

void initQuad(uint * VAOptr, uint * VBOptr, uint * EBOptr) {
    float verts[] = {
        0.0, -1.0, -1.0,   0.0, 0.0,
        0.0,  1.0, -1.0,   1.0, 0.0,
        0.0,  1.0,  1.0,   1.0, 1.0,
        0.0, -1.0,  1.0,   0.0, 1.0
    };
    uint inds[] = {
        0, 1, 2,
        2, 3, 0
    };

    glGenVertexArrays(1, VAOptr);
    glBindVertexArray(*VAOptr);

    glGenBuffers(1, VBOptr);
    glBindBuffer(GL_ARRAY_BUFFER, *VBOptr);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

    glGenBuffers(1, EBOptr);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *EBOptr);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(inds), inds, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)(0 * sizeof(float)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void reshape(GLFWwindow * window, int width, int height) {
    W = width, H = height;
    glViewport(0, 0, W, H);
}

void keyboard(GLFWwindow * window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
    if (key == GLFW_KEY_UP && action == GLFW_PRESS) {
        speed *= 2;
    }
    if (key == GLFW_KEY_DOWN && action == GLFW_PRESS) {
        speed /= 2;
    }
}

void cursor(GLFWwindow * window, double x, double y) {
    phi += sens * (y - prevy);
    psi += sens * (prevx - x);
    phi = glm_clamp(phi, 1, 179);

    dir[0] = sin(glm_rad(phi)) * cos(glm_rad(psi));
    dir[1] = sin(glm_rad(phi)) * sin(glm_rad(psi));
    dir[2] = cos(glm_rad(phi));
    glm_normalize(dir);

    prevy = y, prevx = x;
}

void setCallbacks(GLFWwindow * window) {
    glfwSetWindowSizeCallback(window, reshape);
    glfwSetKeyCallback(window, keyboard);
    glfwSetCursorPosCallback(window, cursor);
}

void computeScene(uint computeprogram) {
    int loc;
    glUseProgram(computeprogram);

    loc = glGetUniformLocation(computeprogram, "eye");
    glUniform3fv(loc, 1, eye);
    loc = glGetUniformLocation(computeprogram, "dir");
    glUniform3fv(loc, 1, dir);
    loc = glGetUniformLocation(computeprogram, "up");
    glUniform3fv(loc, 1, up);

    glDispatchCompute(R, R, 1);
    glUseProgram(0);
}

void renderScene(uint renderprogram) {
    int loc;
    glUseProgram(renderprogram);

    mat4 model, view, proj;

    glm_mat4_identity(model);
    vec3 realeye = {1, 0, 0};
    vec3 realcenter = {0, 0, 0};
    glm_lookat(realeye, realcenter, up, view);
    glm_perspective(glm_rad(45), (float)W/H, 0.1, 100.0, proj);

    mat4 mv, mvp;
    glm_mat4_mul(view, model, mv);
    glm_mat4_mul(proj, mv, mvp);

    loc = glGetUniformLocation(renderprogram, "mvp");
    glUniformMatrix4fv(loc, 1, false, (float *)mvp);

    loc = glGetUniformLocation(renderprogram, "tex");
    glUniform1i(loc, 0);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    glUseProgram(0);
}

void move(GLFWwindow * window) {
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        vec3 front = {dir[0], dir[1], 0};
        glm_normalize(front);
        eye[0] += (curt - prevt) * speed * front[0];
        eye[1] += (curt - prevt) * speed * front[1];
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        vec3 front = {dir[0], dir[1], 0};
        glm_normalize(front);
        eye[0] -= (curt - prevt) * speed * front[0];
        eye[1] -= (curt - prevt) * speed * front[1];
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        vec3 left;
        glm_vec3_cross(up, dir, left);
        glm_normalize(left);
        eye[0] += (curt - prevt) * speed * left[0];
        eye[1] += (curt - prevt) * speed * left[1];
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        vec3 right;
        glm_vec3_cross(dir, up, right);
        glm_normalize(right);
        eye[0] += (curt - prevt) * speed * right[0];
        eye[1] += (curt - prevt) * speed * right[1];
    }
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        eye[2] += (curt - prevt) * speed * 1;
    }
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
        eye[2] -= (curt - prevt) * speed * 1;
    }
}

void run(GLFWwindow * window) {
    uint VAO, VBO, EBO;
    initQuad(&VAO, &VBO, &EBO);
    uint renderprogram = loadRenderProgram("vert.glsl", "frag.glsl");
    uint computeprogram = loadComputeProgram("comp.glsl");
    uint scene = createScene();

    glBindVertexArray(VAO);
    glBindTexture(GL_TEXTURE_2D, scene);

    glfwSetCursorPos(window, prevx, prevy);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    while (!glfwWindowShouldClose(window)) {
        prevt = curt;
        curt = glfwGetTime();

        glClearColor(1.0, 1.0, 1.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);

        computeScene(computeprogram);
        renderScene(renderprogram);

        glfwSwapBuffers(window);
        glfwPollEvents();
        move(window);
    }
}

int main() {
    GLFWwindow * window = initWindow();
    setCallbacks(window);
    run(window);
    glfwTerminate();
    return 0;
}
