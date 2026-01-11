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
#define ERROR(msg, ...) do { fprintf(stderr, "ERROR: "); fprintf(stderr, msg, ##__VA_ARGS__); exit(1); } while(0)

#define OCTAVES 10
#define MAX_HEIGHT 100
#define GRID_WIDTH 500 
#define GRID_HEIGHT 500

typedef struct {
    float aspectRatio;
    float fov;
    float lastX;
    float lastY;
    bool firstMouse;
    float yaw;
    float pitch;
    float sensitivity;
    float speed;

    Vec3 pos;
    Vec3 front;
    Vec3 up;
    Vec3 right;
    
    Mat4 proj;
    Mat4 view;
} Camera;

typedef struct {
    GLFWwindow* window;
    int width;
    int height;
    double mouseX, mouseY;
    
    Camera camera;

    uint32_t shader;
    uint32_t vao, vbo, ebo;
    uint32_t tex;
    uint32_t count;
    
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

void getHeight(uint32_t width, uint32_t height, uint32_t* data) {
    size_t size = width * height;

    float scale = 0.025f;
    for(uint32_t y = 0; y < height; y++) {
        for(uint32_t x = 0; x < width; x++) {
            float noise = getPerlin2D(x * scale, y * scale, OCTAVES);
            noise = noise * 0.5f + 0.5f;
            noise *= 255;

            data[y * width + x] = rgbToInt(noise, noise, noise);
        }
    }
}

bool createShader(Ctx* ctx, uint32_t* id) {
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
        fprintf(stderr, log);
        return false;
    }
    fID = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fID, 1, &fragStr, 0);
    glCompileShader(fID);
    glGetShaderiv(fID, GL_COMPILE_STATUS, &success);
    if(!success) {
        glGetShaderInfoLog(fID, 512, 0, log);
        fprintf(stderr, log);
        return false;
    }

    *id = glCreateProgram();
    glAttachShader(*id, vID);
    glAttachShader(*id, fID);
    glLinkProgram(*id);
    glValidateProgram(*id);
    glGetProgramiv(*id, GL_LINK_STATUS, &success);
    if(!success) {
        glGetProgramInfoLog(*id, 1024, 0, log);
        ERROR(log);
    }
    glGetProgramiv(*id, GL_VALIDATE_STATUS, &success);
    if(!success) {
        glGetProgramInfoLog(*id, 1024, 0, log);
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

void createCamera(Ctx* ctx) {
    ctx->camera.aspectRatio = ((float)ctx->width)/ctx->height;
    ctx->camera.fov = 90.0f;
    ctx->camera.firstMouse = true;
    ctx->camera.lastX = (float)ctx->width/2;
    ctx->camera.lastY = (float)ctx->height/2;
    ctx->camera.yaw = -90.0f;
    ctx->camera.pitch = 0.0f;
    ctx->camera.speed = 0.01f;
    ctx->camera.sensitivity = 0.05f;

    ctx->camera.pos = vec3Create(0, 100, 3);
    ctx->camera.front = vec3Create(0, 0, -1);    
    ctx->camera.up = vec3Create(0, 1, 0);
    ctx->camera.right = vec3Normalize(vec3Cross(ctx->camera.up, ctx->camera.front));

    ctx->camera.proj = mat4Perspective(ctx->camera.fov * DEG2RAD_MULTIPLIER, ctx->camera.aspectRatio, 0.01f, 1000.0f);
    ctx->camera.view = mat4LookAt(ctx->camera.pos, vec3Add(ctx->camera.pos, ctx->camera.front), ctx->camera.up);
}

void updateCamera(Ctx* ctx) {
    bool moved = false;

    //Key Input
    {
        if(glfwGetKey(ctx->window, GLFW_KEY_W) == GLFW_PRESS) {
            ctx->camera.pos = vec3Add(ctx->camera.pos, vec3MulScalar(ctx->camera.front, ctx->camera.speed));
            moved = true;
        }
        if(glfwGetKey(ctx->window, GLFW_KEY_S) == GLFW_PRESS) {
            ctx->camera.pos = vec3Sub(ctx->camera.pos, vec3MulScalar(ctx->camera.front, ctx->camera.speed));
            moved = true;
        }
        if(glfwGetKey(ctx->window, GLFW_KEY_A) == GLFW_PRESS) {
            ctx->camera.pos = vec3Add(ctx->camera.pos, vec3MulScalar(ctx->camera.right, ctx->camera.speed));
            moved = true;
        }
        if(glfwGetKey(ctx->window, GLFW_KEY_D) == GLFW_PRESS) {
            ctx->camera.pos = vec3Sub(ctx->camera.pos, vec3MulScalar(ctx->camera.right, ctx->camera.speed));
            moved = true;
        }
        if(glfwGetKey(ctx->window, GLFW_KEY_SPACE) == GLFW_PRESS) {
            ctx->camera.pos = vec3Add(ctx->camera.pos, vec3MulScalar(ctx->camera.up, ctx->camera.speed));
            moved = true;
        }
        if(glfwGetKey(ctx->window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
            ctx->camera.pos = vec3Sub(ctx->camera.pos, vec3MulScalar(ctx->camera.up, ctx->camera.speed));
            moved = true;
        }
    }

    // Mouse Input
    {
        if(glfwGetMouseButton(ctx->window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
            double xpos = ctx->mouseX;
            double ypos = ctx->mouseY;

            if(ctx->camera.firstMouse) {
                glfwSetInputMode(ctx->window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                ctx->camera.lastX = xpos;
                ctx->camera.lastY = ypos;
                ctx->camera.firstMouse = false;
            }

            float xDelta = (xpos - ctx->camera.lastX);
            float yDelta = (ctx->camera.lastY - ypos);

            ctx->camera.lastX = xpos;
            ctx->camera.lastY = ypos;

            if(xDelta != 0 || yDelta != 0) {
                xDelta *= ctx->camera.sensitivity;
                yDelta *= ctx->camera.sensitivity;

                ctx->camera.yaw += xDelta;
                ctx->camera.pitch += yDelta;
                
                if(ctx->camera.pitch > 89.9f)
                    ctx->camera.pitch = 89.9f;
                if(ctx->camera.pitch < -89.9f)
                    ctx->camera.pitch = -89.9f;

                Vec3 front;
                front.x = cos(ctx->camera.yaw * DEG2RAD_MULTIPLIER) * cos(ctx->camera.pitch * DEG2RAD_MULTIPLIER);
                front.y = sin(ctx->camera.pitch * DEG2RAD_MULTIPLIER);
                front.z = sin(ctx->camera.yaw * DEG2RAD_MULTIPLIER) * cos(ctx->camera.pitch * DEG2RAD_MULTIPLIER);
                
                ctx->camera.front = vec3Normalize(front);

                moved = true;
            }
        }
        else if(glfwGetMouseButton(ctx->window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_RELEASE) {
            glfwSetInputMode(ctx->window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            ctx->camera.firstMouse = true;
        }
    }
    // Updating matrices
    {
        ctx->camera.right = vec3Normalize(vec3Cross(vec3Create(0, 1, 0), ctx->camera.front));
        ctx->camera.up = vec3Normalize(vec3Cross(ctx->camera.front, ctx->camera.right));

        if(!moved)
            return;
        
        ctx->camera.proj = mat4Perspective(ctx->camera.fov * DEG2RAD_MULTIPLIER, ctx->camera.aspectRatio, 0.01f, 1000.0f);
        ctx->camera.view = mat4LookAt(ctx->camera.pos, vec3Add(ctx->camera.pos, ctx->camera.front), ctx->camera.up);
    }
}

void createTerrain(Ctx* ctx) {
    size_t size = GRID_HEIGHT * GRID_WIDTH * 3 * sizeof(float);
    float* data = malloc(size);
    memset(data, 0, size);
    int idx = 0;
    for(uint32_t y = 0; y < GRID_HEIGHT; y++) {
        for(uint32_t x = 0; x < GRID_WIDTH; x++) {
            float x1 = (x - (GRID_WIDTH - 1)/2.0f);
            float z1 = (y - (GRID_HEIGHT - 1)/2.0f);
            
            uint32_t noise = ctx->data[y * GRID_WIDTH + x] & 0xff;
            float y1 = noise / 255.0f;
            y1 *= MAX_HEIGHT;

            data[idx++] = x1;
            data[idx++] = y1;
            data[idx++] = z1;
        }
    }

    uint32_t indicesLen = (GRID_WIDTH - 1) * (GRID_HEIGHT - 1) * 6;
    ctx->count = indicesLen;
    uint32_t* indices = malloc(indicesLen * sizeof(uint32_t));
    memset(indices, 0, indicesLen * sizeof(uint32_t));

    idx = 0;
    for(uint32_t y = 0; y < GRID_HEIGHT-1; y++) {
        for(uint32_t x = 0; x < GRID_WIDTH-1; x++) {
            uint32_t v0 = y * GRID_WIDTH + x;
            uint32_t v1 = y * GRID_WIDTH + (x + 1);
            uint32_t v2 = (y+1) * GRID_WIDTH + x;
            uint32_t v3 = (y+1) * GRID_WIDTH + (x+1);

            indices[idx++] = v0;
            indices[idx++] = v2;
            indices[idx++] = v1;
            
            indices[idx++] = v1;
            indices[idx++] = v2;
            indices[idx++] = v3;
        }
    }

    glGenVertexArrays(1, &ctx->vao);
    glGenBuffers(1, &ctx->vbo);
    glGenBuffers(1, &ctx->ebo);

    glBindVertexArray(ctx->vao);

    glBindBuffer(GL_ARRAY_BUFFER, ctx->vbo);
    glBufferData(GL_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ctx->ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * ctx->count, indices, GL_STATIC_DRAW);

    glBindVertexArray(0);

    free(indices);
    free(data);
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

            ctx.window = glfwCreateWindow(ctx.width, ctx.height, "PerlinTerrain", 0, 0);
            if(!ctx.window)
                ERROR("Couldn't create glfw window!\n");
            glfwMakeContextCurrent(ctx.window);
            if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
                ERROR("Couldn't load opengl!\n");
        }
        //Height map 
        {
            ctx.data = malloc(GRID_HEIGHT * GRID_WIDTH * sizeof(uint32_t));
            memset(ctx.data, 0, sizeof(uint32_t) * GRID_WIDTH * GRID_HEIGHT);
            getHeight(GRID_WIDTH, GRID_HEIGHT, ctx.data);
        }
        // Texture 
        {
            glGenTextures(1, &ctx.tex);
            glBindTexture(GL_TEXTURE_2D, ctx.tex);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GRID_WIDTH, GRID_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, ctx.data);

            glGenerateMipmap(GL_TEXTURE_2D);

            glBindTexture(GL_TEXTURE_2D, 0);
        }
        //Shader
        if(!createShader(&ctx, &ctx.shader))
            exit(1);
        // Buffers 
        createTerrain(&ctx);
        // Camera
        createCamera(&ctx);
    }

    //Main loop
    glfwShowWindow(ctx.window);
    glViewport(0, 0, ctx.width, ctx.height);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    while(!glfwWindowShouldClose(ctx.window)) {
        // Render
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(ctx.shader);
        putMat4Shader(ctx.shader, "u_Proj", ctx.camera.proj);
        putMat4Shader(ctx.shader, "u_View", ctx.camera.view);
        glUniform2f(glGetUniformLocation(ctx.shader, "u_TexRes"), (float)GRID_WIDTH, (float)GRID_HEIGHT);
        glUniform1f(glGetUniformLocation(ctx.shader, "u_MaxHeight"), (float)MAX_HEIGHT);
        glUniform3f(glGetUniformLocation(ctx.shader, "u_LightPos"), 1000.0f, 1000.0f, 0.0f);
        
        glBindTexture(GL_TEXTURE_2D, ctx.tex);
        glActiveTexture(GL_TEXTURE0);
        glUniform1i(glGetUniformLocation(ctx.shader, "u_Tex"), 0);

        glBindVertexArray(ctx.vao);
        glDrawElements(GL_TRIANGLES, ctx.count, GL_UNSIGNED_INT, 0);
        
        // Update
        if(glfwGetKey(ctx.window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            break;
        }
        if(glfwGetKey(ctx.window, GLFW_KEY_R) == GLFW_PRESS) {
            //glDeleteProgram(ctx.shader);
            uint32_t id;
            if(createShader(&ctx, &id)) {
                glDeleteProgram(ctx.shader);
                ctx.shader = id;
            }
        }
        glfwGetCursorPos(ctx.window, &ctx.mouseX, &ctx.mouseY);
        glfwGetWindowSize(ctx.window, &ctx.width, &ctx.height);
        glViewport(0, 0, ctx.width, ctx.height);

        updateCamera(&ctx);

        glfwSwapBuffers(ctx.window);
        glfwPollEvents();
    }

    // Cleanup
    {
        free(ctx.data);

        glDeleteTextures(1, &ctx.tex);

        glDeleteBuffers(1, &ctx.ebo);
        glDeleteBuffers(1, &ctx.vbo);
        glDeleteVertexArrays(1, &ctx.vao);

        glDeleteProgram(ctx.shader);

        glfwDestroyWindow(ctx.window);
        glfwTerminate();
    }

    return 0;
}
