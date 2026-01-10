#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stb/stb_perlin.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "maths.h"

#define ARR_LEN(arr) (sizeof(arr)/sizeof(arr[0]))

#define INFO(msg, ...) do { fprintf(stdout, "INFO: "); fprintf(stdout, msg, ##__VA_ARGS__); } while(0)
#define ERROR(msg, ...) do { fprintf(stderr, "ERROR :"); fprintf(stderr, msg, ##__VA_ARGS__); exit(1); } while(0)

typedef struct {
    GLFWwindow* window;
    int width;
    int height;

    uint32_t shader;
    uint32_t vao, vbo;
    uint32_t tex;
    
    uint32_t* data;
} Ctx;

char* readFile(const char* path) {
    FILE* file = fopen(path, "rt");
    if(!file)
        ERROR("Can't read file :- %s\n", path);

    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    rewind(file);

    char* str = malloc(sizeof(char) * (size + 1));
    memset(str, 0, sizeof(char) * (size + 1));
    
    fread(str, size, 1, file);
    str[size] = '\0';

    fclose(file);
    
    return str;
}

uint32_t rgbToInt(uint8_t r, uint8_t b, uint8_t g) {
    return (uint32_t)((0xFF << 24) | (b << 16) | (g << 8) | r);
}

float getPerlin2D(float x, float y, int octaves) {
    float v = 0.0f;
    float amplitude = 1.0f;
    float frequency = 1.0f;
    float max = 0.0f;

    for(int i = 0; i < octaves; i++) {
        v += stb_perlin_noise3(x * frequency, y * frequency, 0, 0, 0, 0) * amplitude;
        max += amplitude;
        frequency *= 2.0f;
        amplitude *= 0.5f;
    }
    
    return v/max;
}

void getTextureColor(uint32_t width, uint32_t height, uint32_t* data) {
    size_t size = width * height;

    float scale = 0.01f;
    for(uint32_t y = 0; y < height; y++) {
        for(uint32_t x = 0; x < width; x++) {
            float noise = getPerlin2D(x * scale, y * scale, 4);
            noise = noise * 0.5f + 0.5f;
            noise *= 255;

            data[y * width + x] = rgbToInt(noise, noise, noise);
        }
    }
}

void createShader(Ctx* ctx) {
    char log[512];
    int success = false;
    const char* vertStr = readFile("shaders/default.vert");
    const char* fragStr = readFile("shaders/default.frag");

    uint32_t vID, fID;
    vID = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vID, 1, &vertStr, 0);
    glCompileShader(vID);
    glGetShaderiv(vID, GL_COMPILE_STATUS, &success);
    if(!success) {
        glGetShaderInfoLog(vID, 512, 0, log);
        ERROR(log);
    }
    fID = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fID, 1, &fragStr, 0);
    glCompileShader(fID);
    glGetShaderiv(fID, GL_COMPILE_STATUS, &success);
    if(!success) {
        glGetShaderInfoLog(fID, 512, 0, log);
        ERROR(log);
    }

    ctx->shader = glCreateProgram();
    glAttachShader(ctx->shader, vID);
    glAttachShader(ctx->shader, fID);
    glLinkProgram(ctx->shader);
    glValidateProgram(ctx->shader);
    glGetProgramiv(ctx->shader, GL_LINK_STATUS, &success);
    if(!success) {
        glGetProgramInfoLog(ctx->shader, 1024, 0, log);
        ERROR(log);
    }
    glGetProgramiv(ctx->shader, GL_VALIDATE_STATUS, &success);
    if(!success) {
        glGetProgramInfoLog(ctx->shader, 1024, 0, log);
        ERROR(log);
    }

    glDeleteShader(vID);
    glDeleteShader(fID);

    free((void*)vertStr);
    free((void*)fragStr);
}

void putMat4Shader(uint32_t id, const char* name, Mat4 mat) {
    glUseProgram(id);
    glUniformMatrix4fv(glGetUniformLocation(id, name), 1, GL_FALSE, &mat.data[0]);
}

int main(void) {
    Ctx ctx = {
        .width = 1200,
        .height = 720
    };

    // Init
    {
        // Window
        {
            if(!glfwInit())
                ERROR("Couldnt't init glfw!\n");
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
            glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
            glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

            ctx.window = glfwCreateWindow(ctx.width, ctx.height, "PerlinTerrain", 0, 0);
            if(!ctx.window)
                ERROR("Couldn't create glfw window!\n");
            glfwMakeContextCurrent(ctx.window);
            if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
                ERROR("Couldn't load opengl!\n");
        }
        //Shader
        createShader(&ctx);
        // Buffers 
        {
            float data[] = {
                // pos                      uv
                -1.0f,  1.0f, 0.0f,     0.0f, 1.0f,
                -1.0f, -1.0f, 0.0f,     0.0f, 0.0f,
                 1.0f, -1.0f, 0.0f,     1.0f, 0.0f,
                -1.0f,  1.0f, 0.0f,     0.0f, 1.0f,
                 1.0f, -1.0f, 0.0f,     1.0f, 0.0f,
                 1.0f,  1.0f, 0.0f,     1.0f, 1.0f
            };

            glGenVertexArrays(1, &ctx.vao);
            glGenBuffers(1, &ctx.vbo);

            glBindVertexArray(ctx.vao);

            glBindBuffer(GL_ARRAY_BUFFER, ctx.vbo);
            glBufferData(GL_ARRAY_BUFFER, sizeof(float) * ARR_LEN(data), data, GL_STATIC_DRAW);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(sizeof(float) * 3));
            glEnableVertexAttribArray(1);

            glBindVertexArray(0);
        }
        // Texture
        {
            ctx.data = malloc(sizeof(uint32_t) * ctx.width * ctx.height);
            memset(ctx.data, 0, ctx.width * ctx.height);
            getTextureColor(ctx.width, ctx.height, ctx.data);

            glGenTextures(1, &ctx.tex);
            glBindTexture(GL_TEXTURE_2D, ctx.tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ctx.width, ctx.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, ctx.data);

            glGenerateMipmap(GL_TEXTURE_2D);

            glBindTexture(GL_TEXTURE_2D, 0);
        }
    }

    //Main loop
    glfwShowWindow(ctx.window);
    glViewport(0, 0, ctx.width, ctx.height);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    while(!glfwWindowShouldClose(ctx.window)) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(ctx.shader);
        glBindTexture(GL_TEXTURE_2D, ctx.tex);
        glActiveTexture(GL_TEXTURE0);
        glUniform1i(glGetUniformLocation(ctx.shader, "u_Tex"), 0);

        glBindVertexArray(ctx.vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        if(glfwGetKey(ctx.window, GLFW_KEY_R) == GLFW_PRESS) {
            glDeleteProgram(ctx.shader);
            createShader(&ctx);
        }

        glfwSwapBuffers(ctx.window);
        glfwPollEvents();
    }

    // Cleanup
    {
        free(ctx.data);
        glDeleteTextures(1, &ctx.tex);

        glDeleteBuffers(1, &ctx.vbo);
        glDeleteVertexArrays(1, &ctx.vao);

        glDeleteProgram(ctx.shader);

        glfwDestroyWindow(ctx.window);
        glfwTerminate();
    }

    return 0;
}
