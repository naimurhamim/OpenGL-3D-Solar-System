#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "Shader.h"
#include "Sphere.h"
#include "OrbitRing.h"

// ==================== GLOBAL CAMERA & SIM STATE ====================

float camYaw      = 0.0f;
float camPitch    = 20.0f;
float camDistance = 120.0f;
float simSpeed    = 1.0f;
float cometAngle  = 0.0f;
bool  paused      = false;
bool  showHelp    = false;

int   followPlanet  = -1;
float followEyeDist = 15.0f;

float smoothTargetX = 0.0f;
float smoothTargetZ = 0.0f;

float planetWorldX[8] = {0};
float planetWorldZ[8] = {0};

bool   mouseDown  = false;
double lastMouseX = 0, lastMouseY = 0;

float deltaTime  = 0.0f;
float lastFrameT = 0.0f;

// Spacecraft
bool  spacecraftMode = false;
float scX = 0.0f, scY = 5.0f, scZ = 50.0f;
float scYaw = 0.0f, scPitch = 0.0f;
float scSpeed    = 0.0f;
float scMaxSpeed = 30.0f;

// LOD distances
float lodHighDist   = 50.0f;
float lodMediumDist = 120.0f;

int winW = 1200, winH = 800;

// ==================== MATH HELPERS ====================

struct Mat4
{
    float m[16] = {0};
    Mat4() { m[0] = m[5] = m[10] = m[15] = 1.0f; }
};

Mat4 multiply(const Mat4 &a, const Mat4 &b)
{
    Mat4 r;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
        {
            r.m[j*4+i] = 0;
            for (int k = 0; k < 4; k++)
                r.m[j*4+i] += a.m[k*4+i] * b.m[j*4+k];
        }
    return r;
}

Mat4 perspective(float fovY, float aspect, float nearP, float farP)
{
    Mat4 r;
    float f  = 1.0f / tanf(fovY / 2.0f);
    r.m[0]   = f / aspect;
    r.m[5]   = f;
    r.m[10]  = -(farP + nearP) / (farP - nearP);
    r.m[11]  = -1;
    r.m[14]  = -(2 * farP * nearP) / (farP - nearP);
    r.m[15]  = 0;
    return r;
}

Mat4 translate(float x, float y, float z)
{
    Mat4 r;
    r.m[12] = x; r.m[13] = y; r.m[14] = z;
    return r;
}

Mat4 scale(float x, float y, float z)
{
    Mat4 r;
    r.m[0] = x; r.m[5] = y; r.m[10] = z;
    return r;
}

Mat4 rotateY(float angle)
{
    Mat4 r;
    r.m[0]  =  cosf(angle); r.m[8]  = sinf(angle);
    r.m[2]  = -sinf(angle); r.m[10] = cosf(angle);
    return r;
}

Mat4 rotateX(float angle)
{
    Mat4 r;
    r.m[5]  =  cosf(angle); r.m[9]  = -sinf(angle);
    r.m[6]  =  sinf(angle); r.m[10] =  cosf(angle);
    return r;
}

Mat4 lookAt(float eyeX, float eyeY, float eyeZ,
            float cx,   float cy,   float cz)
{
    float fx = cx-eyeX, fy = cy-eyeY, fz = cz-eyeZ;
    float len = sqrtf(fx*fx + fy*fy + fz*fz);
    if (len < 0.0001f) len = 0.0001f;
    fx /= len; fy /= len; fz /= len;

    float rx = -fz, ry = 0.0f, rz = fx;
    float rl = sqrtf(rx*rx + ry*ry + rz*rz);
    if (rl > 0.0001f) { rx /= rl; ry /= rl; rz /= rl; }

    float ux = ry*fz - rz*fy;
    float uy = rz*fx - rx*fz;
    float uz = rx*fy - ry*fx;

    Mat4 r;
    r.m[0]  =  rx; r.m[4]  =  ry; r.m[8]  =  rz;
    r.m[1]  =  ux; r.m[5]  =  uy; r.m[9]  =  uz;
    r.m[2]  = -fx; r.m[6]  = -fy; r.m[10] = -fz;
    r.m[12] = -(rx*eyeX + ry*eyeY + rz*eyeZ);
    r.m[13] = -(ux*eyeX + uy*eyeY + uz*eyeZ);
    r.m[14] =  (fx*eyeX + fy*eyeY + fz*eyeZ);
    return r;
}

float lerp(float a, float b, float t) { return a + (b-a)*t; }

float dist3(float ax, float ay, float az,
            float bx, float by, float bz)
{
    float dx=ax-bx, dy=ay-by, dz=az-bz;
    return sqrtf(dx*dx + dy*dy + dz*dz);
}

// ==================== TEXTURE LOADER ====================

unsigned int loadTexture(const char *path)
{
    unsigned int id;
    glGenTextures(1, &id);
    int w, h, ch;
    stbi_set_flip_vertically_on_load(true);
    unsigned char *data = stbi_load(path, &w, &h, &ch, 0);
    if (data)
    {
        GLenum fmt = (ch == 4) ? GL_RGBA : GL_RGB;
        glBindTexture(GL_TEXTURE_2D, id);
        glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        printf("Loaded: %s (%dx%d ch:%d)\n", path, w, h, ch);
    }
    else { printf("FAILED: %s\n", path); }
    stbi_image_free(data);
    return id;
}

// ==================== LOD SPHERE DRAW ====================

void drawLOD(Sphere &hi, Sphere &med, Sphere &lo,
             float eyeX, float eyeY, float eyeZ,
             float px, float pz)
{
    float d = dist3(eyeX, eyeY, eyeZ, px, 0.0f, pz);
    if      (d < lodHighDist)   hi.draw();
    else if (d < lodMediumDist) med.draw();
    else                        lo.draw();
}

// ==================== DRAW HELPERS ====================

void drawStars(Shader &shader, Sphere &sphere)
{
    srand(42);
    for (int i = 0; i < 800; i++)
    {
        float x = (float)(rand() % 2000) - 1000.0f;
        float y = (float)(rand() % 2000) - 1000.0f;
        float z = (float)(rand() % 2000) - 1000.0f;
        if (x*x + y*y + z*z < 200*200) continue;
        float b  = 0.5f + (rand() % 50) / 100.0f;
        float sz = 0.2f + (rand() % 30) / 100.0f;
        shader.setVec3("lightColor", b, b, b * 0.95f);
        Mat4 model = multiply(translate(x, y, z), scale(sz, sz, sz));
        shader.setMat4("model", model.m);
        shader.setBool("isSun",    true);
        shader.setBool("isEarth",  false);
        shader.setBool("isClouds", false);
        sphere.draw();
    }
    shader.setVec3("lightColor", 1.0f, 1.0f, 0.95f);
}

void drawAsteroidBelt(Shader &shader, Sphere &sphere, float time, float speed)
{
    srand(123);
    float PI = 3.14159f;
    for (int i = 0; i < 400; i++)
    {
        float r     = 38.0f + (rand() % 1000) / 400.0f;
        float angle = 2.0f * PI * (rand() % 1000) / 1000.0f + time * speed * 0.08f;
        float y     = ((rand() % 100) - 50) / 800.0f;
        float sz    = 0.02f + (rand() % 100) / 5000.0f;
        float grey  = 0.3f + (rand() % 40) / 100.0f;
        shader.setVec3("lightColor", grey, grey * 0.9f, grey * 0.8f);
        Mat4 model = multiply(translate(r * cosf(angle), y, r * sinf(angle)),
                              scale(sz, sz, sz));
        shader.setMat4("model", model.m);
        shader.setBool("isSun",    true);
        shader.setBool("isEarth",  false);
        shader.setBool("isClouds", false);
        sphere.draw();
    }
    shader.setVec3("lightColor", 1.0f, 1.0f, 0.95f);
}

void drawComet(Shader &shader, Sphere &sphere, float angle, unsigned int texComet)
{
    float PI = 3.14159f, orbitA = 70.0f, orbitB = 30.0f;

    // Orbit path dots
    for (int i = 0; i < 120; i++)
    {
        float a = 2.0f * PI * i / 120;
        Mat4 model = multiply(translate(orbitA*cosf(a), 0.0f, orbitB*sinf(a)),
                              scale(0.02f, 0.02f, 0.02f));
        shader.setVec3("lightColor", 0.25f, 0.35f, 0.45f);
        shader.setMat4("model", model.m);
        shader.setBool("isSun", true);
        shader.setBool("isEarth", false);
        shader.setBool("isClouds", false);
        sphere.draw();
    }
    shader.setVec3("lightColor", 1.0f, 1.0f, 0.95f);

    float cx = orbitA * cosf(angle);
    float cz = orbitB * sinf(angle);
    float tlen = sqrtf(cx*cx + cz*cz);
    float tdx  = (tlen > 0.001f) ? cx/tlen : 1.0f;
    float tdz  = (tlen > 0.001f) ? cz/tlen : 0.0f;

    // Tail
    for (int t = 1; t <= 20; t++)
    {
        float alpha = 1.0f - (float)t / 20.0f;
        float sz    = 0.15f * alpha;
        Mat4 model  = multiply(translate(cx + tdx*t, 0.0f, cz + tdz*t),
                               scale(sz, sz*0.5f, sz));
        shader.setVec3("lightColor", alpha*0.7f, alpha*0.85f, alpha*1.0f);
        shader.setMat4("model", model.m);
        shader.setBool("isSun", true);
        shader.setBool("isEarth", false);
        shader.setBool("isClouds", false);
        sphere.draw();
    }
    shader.setVec3("lightColor", 1.0f, 1.0f, 0.95f);

    // Head
    Mat4 model = multiply(translate(cx, 0.0f, cz), scale(0.4f, 0.4f, 0.4f));
    shader.setMat4("model", model.m);
    shader.setBool("isSun", true);
    shader.setBool("isEarth", false);
    shader.setBool("isClouds", false);
    glBindTexture(GL_TEXTURE_2D, texComet);
    sphere.draw();
}

// ==================== CALLBACKS ====================

void mouseButtonCB(GLFWwindow *w, int btn, int action, int mods)
{
    if (btn == GLFW_MOUSE_BUTTON_LEFT)
    {
        mouseDown = (action == GLFW_PRESS);
        glfwGetCursorPos(w, &lastMouseX, &lastMouseY);
    }
}

void cursorPosCB(GLFWwindow *w, double x, double y)
{
    if (!mouseDown) return;
    float dx = (float)(x - lastMouseX) * 0.3f;
    float dy = (float)(y - lastMouseY) * 0.3f;
    if (spacecraftMode)
    {
        scYaw   -= dx * 0.5f;
        scPitch -= dy * 0.5f;
        if (scPitch >  89.0f) scPitch =  89.0f;
        if (scPitch < -89.0f) scPitch = -89.0f;
    }
    else
    {
        camYaw   += dx;
        camPitch += dy;
        if (camPitch >  89.0f) camPitch =  89.0f;
        if (camPitch < -89.0f) camPitch = -89.0f;
    }
    lastMouseX = x; lastMouseY = y;
}

void scrollCB(GLFWwindow *w, double xoff, double yoff)
{
    if (spacecraftMode)
    {
        scSpeed += (float)yoff * 2.0f;
        if (scSpeed < -scMaxSpeed) scSpeed = -scMaxSpeed;
        if (scSpeed >  scMaxSpeed) scSpeed =  scMaxSpeed;
        return;
    }
    if (followPlanet >= 0)
    {
        followEyeDist -= (float)yoff * 2.0f;
        if (followEyeDist <   1.5f) followEyeDist =   1.5f;
        if (followEyeDist > 200.0f) followEyeDist = 200.0f;
    }
    else
    {
        // Exponential zoom — fast when far, slow when close
        camDistance *= (1.0f - (float)yoff * 0.08f);
        if (camDistance <   1.0f) camDistance =   1.0f;
        if (camDistance > 800.0f) camDistance = 800.0f;
    }
}

void framebufferSizeCB(GLFWwindow *w, int width, int height)
{
    winW = width; winH = height;
    glViewport(0, 0, width, height);
}

void keyCB(GLFWwindow *w, int key, int scancode, int action, int mods)
{
    if (action == GLFW_PRESS || action == GLFW_REPEAT)
    {
        if (key == GLFW_KEY_P)     paused = true;
        if (key == GLFW_KEY_R)     paused = false;
        if (key == GLFW_KEY_EQUAL) simSpeed += 0.2f;
        if (key == GLFW_KEY_MINUS && !spacecraftMode)
        {
            simSpeed -= 0.2f;
            if (simSpeed < 0.0f) simSpeed = 0.0f;
        }
        if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(w, true);
        if (key == GLFW_KEY_I)      showHelp = !showHelp;
    }

    if (action == GLFW_PRESS)
    {
        // Spacecraft toggle
        if (key == GLFW_KEY_V)
        {
            spacecraftMode = !spacecraftMode;
            if (spacecraftMode)
            {
                float yr = camYaw   * 3.14159f / 180.0f;
                float pr = camPitch * 3.14159f / 180.0f;
                scX = camDistance * cosf(pr) * sinf(yr);
                scY = camDistance * sinf(pr);
                scZ = camDistance * cosf(pr) * cosf(yr);
                scYaw = camYaw; scPitch = camPitch;
                scSpeed = 0.0f; followPlanet = -1;
                printf("Spacecraft ON — WASD=fly, Q/E=up/down, scroll=throttle\n");
            }
            else printf("Spacecraft OFF\n");
        }

        if (!spacecraftMode)
        {
            if (key == GLFW_KEY_F)  followPlanet = -1;
            if (key == GLFW_KEY_0)  followPlanet = -1;
            if (key == GLFW_KEY_1)  followPlanet = 0;
            if (key == GLFW_KEY_2)  followPlanet = 1;
            if (key == GLFW_KEY_3)  followPlanet = 2;
            if (key == GLFW_KEY_4)  followPlanet = 3;
            if (key == GLFW_KEY_5)  followPlanet = 4;
            if (key == GLFW_KEY_6)  followPlanet = 5;
            if (key == GLFW_KEY_7)  followPlanet = 6;
            if (key == GLFW_KEY_8)  followPlanet = 7;
        }
    }
}

void printHelp()
{
    printf("\n===== 3D SOLAR SYSTEM CONTROLS =====\n");
    printf("  Mouse drag     : Rotate camera\n");
    printf("  Scroll         : Zoom (exponential)\n");
    printf("  1-8            : Follow planet\n");
    printf("  0 / F          : Free camera\n");
    printf("  P / R          : Pause / Resume\n");
    printf("  = / -          : Speed up / down\n");
    printf("  V              : Toggle spacecraft mode\n");
    printf("  WASD + Q/E     : Fly (spacecraft)\n");
    printf("  Scroll         : Throttle (spacecraft)\n");
    printf("  I              : Toggle this help\n");
    printf("  ESC            : Exit\n");
    printf("=====================================\n\n");
}

// ==================== PLANET DATA ====================

struct Planet
{
    const char  *name;
    float        orbitRadius;
    float        orbitSpeed;
    float        size;
    float        tilt;
    unsigned int texture;
    float        orbitAngle;
    float        selfAngle;
    const char  *type;
    int          moons;
    float        dayLength;
};

float keplerSpeed(float baseSpeed, float orbitRadius)
{
    return baseSpeed * sqrtf(27.0f / orbitRadius);
}

// ==================== MAIN ====================

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow *window = glfwCreateWindow(1200, 800,
        "3D Solar System - Naimur Rashid", NULL, NULL);
    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    glfwSetMouseButtonCallback(window,     mouseButtonCB);
    glfwSetCursorPosCallback(window,       cursorPosCB);
    glfwSetScrollCallback(window,          scrollCB);
    glfwSetKeyCallback(window,             keyCB);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCB);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    Shader shader    ("shaders/planet.vert", "shaders/planet.frag");
    Shader lineShader("shaders/line.vert",   "shaders/line.frag");

    unsigned int texSun         = loadTexture("textures/sun.jpg");
    unsigned int texMercury     = loadTexture("textures/mercury.jpg");
    unsigned int texVenus       = loadTexture("textures/venus.jpg");
    unsigned int texEarth       = loadTexture("textures/earth.jpg");
    unsigned int texMars        = loadTexture("textures/mars.jpg");
    unsigned int texJupiter     = loadTexture("textures/jupiter.jpg");
    unsigned int texSaturn      = loadTexture("textures/saturn.jpg");
    unsigned int texUranus      = loadTexture("textures/uranus.jpg");
    unsigned int texNeptune     = loadTexture("textures/neptune.jpg");
    unsigned int texEarthClouds = loadTexture("textures/earth_clouds.jpg");
    unsigned int texComet       = loadTexture("textures/mercury.jpg");
    unsigned int texMoon        = loadTexture("textures/mercury.jpg");
    unsigned int texSaturnRing  = loadTexture("textures/saturn_ring.png");

    // 3 LOD levels
    Sphere sphereHi (64, 64);
    Sphere sphereMed(32, 32);
    Sphere sphereLo (16, 16);

    OrbitRing orbitRing(360);

    Planet planets[] = {
        {"Mercury", 14.0f, 1.80f, 0.5f, 0.03f, texMercury, 0.0f, 0.0f, "Rocky",      0,  58.6f},
        {"Venus",   20.0f, 1.50f, 0.9f, 3.10f, texVenus,   1.0f, 0.0f, "Rocky",      0, 243.0f},
        {"Earth",   27.0f, 1.00f, 1.0f, 0.41f, texEarth,   2.0f, 0.0f, "Rocky",      1,   1.0f},
        {"Mars",    35.0f, 0.80f, 0.7f, 0.44f, texMars,    3.0f, 0.0f, "Rocky",      2,   1.03f},
        {"Jupiter", 50.0f, 0.50f, 2.5f, 0.05f, texJupiter, 1.5f, 0.0f, "Gas Giant", 95,   0.41f},
        {"Saturn",  65.0f, 0.40f, 2.0f, 0.47f, texSaturn,  2.5f, 0.0f, "Gas Giant",146,   0.44f},
        {"Uranus",  78.0f, 0.30f, 1.5f, 1.71f, texUranus,  4.0f, 0.0f, "Ice Giant", 28,   0.72f},
        {"Neptune", 90.0f, 0.20f, 1.4f, 0.49f, texNeptune, 5.0f, 0.0f, "Ice Giant", 16,   0.67f},
    };
    int planetCount = sizeof(planets) / sizeof(Planet);
    float sunSize = 2.0f;

    printHelp();

    while (!glfwWindowShouldClose(window))
    {
        float currentTime = (float)glfwGetTime();
        deltaTime  = currentTime - lastFrameT;
        lastFrameT = currentTime;
        if (deltaTime > 0.05f) deltaTime = 0.05f;

        glClearColor(0.0f, 0.0f, 0.015f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Update planets
        for (int i = 0; i < planetCount; i++)
        {
            if (!paused)
            {
                planets[i].orbitAngle += keplerSpeed(planets[i].orbitSpeed,
                                                     planets[i].orbitRadius)
                                         * deltaTime * simSpeed;
                planets[i].selfAngle  += 0.5f * deltaTime * simSpeed;
            }
            planetWorldX[i] = planets[i].orbitRadius * cosf(planets[i].orbitAngle);
            planetWorldZ[i] = planets[i].orbitRadius * sinf(planets[i].orbitAngle);
        }
        if (!paused) cometAngle += 0.25f * deltaTime * simSpeed;

        // Camera / spacecraft
        float eyeX, eyeY, eyeZ;
        float lookAtX = 0.0f, lookAtY = 0.0f, lookAtZ = 0.0f;

        if (spacecraftMode)
        {
            float yr  = scYaw   * 3.14159f / 180.0f;
            float pr  = scPitch * 3.14159f / 180.0f;
            float fwdX = cosf(pr) * sinf(yr);
            float fwdY = sinf(pr);
            float fwdZ = cosf(pr) * cosf(yr);
            float rtX  =  cosf(yr);
            float rtZ  = -sinf(yr);
            float mv   = 20.0f * deltaTime;

            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            { scX += fwdX*mv; scY += fwdY*mv; scZ += fwdZ*mv; }
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            { scX -= fwdX*mv; scY -= fwdY*mv; scZ -= fwdZ*mv; }
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            { scX -= rtX*mv; scZ -= rtZ*mv; }
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            { scX += rtX*mv; scZ += rtZ*mv; }
            if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) scY -= mv;
            if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) scY += mv;

            scX += fwdX * scSpeed * deltaTime;
            scY += fwdY * scSpeed * deltaTime;
            scZ += fwdZ * scSpeed * deltaTime;

            eyeX = scX; eyeY = scY; eyeZ = scZ;
            lookAtX = scX + fwdX;
            lookAtY = scY + fwdY;
            lookAtZ = scZ + fwdZ;
        }
        else
        {
            float tX = 0.0f, tZ = 0.0f;
            if (followPlanet >= 0 && followPlanet < 8)
            { tX = planetWorldX[followPlanet]; tZ = planetWorldZ[followPlanet]; }

            float ls = 1.0f - powf(0.92f, deltaTime * 60.0f);
            smoothTargetX = lerp(smoothTargetX, tX, ls);
            smoothTargetZ = lerp(smoothTargetZ, tZ, ls);

            float yr = camYaw   * 3.14159f / 180.0f;
            float pr = camPitch * 3.14159f / 180.0f;

            if (followPlanet >= 0 && followPlanet < 8)
            {
                eyeX = smoothTargetX + followEyeDist * cosf(pr) * sinf(yr);
                eyeY =                 followEyeDist * sinf(pr);
                eyeZ = smoothTargetZ + followEyeDist * cosf(pr) * cosf(yr);
            }
            else
            {
                eyeX = camDistance * cosf(pr) * sinf(yr);
                eyeY = camDistance * sinf(pr);
                eyeZ = camDistance * cosf(pr) * cosf(yr);
            }
            lookAtX = smoothTargetX;
            lookAtZ = smoothTargetZ;
        }

        // Dynamic near/far plane — lets you zoom to tiny dot or huge distance
        float eyeDist  = sqrtf(eyeX*eyeX + eyeY*eyeY + eyeZ*eyeZ);
        float farPlane  = fmaxf(1000.0f, eyeDist * 6.0f);
        float nearPlane = fmaxf(0.01f,   eyeDist * 0.0001f);

        Mat4 view       = lookAt(eyeX, eyeY, eyeZ, lookAtX, lookAtY, lookAtZ);
        Mat4 projection = perspective(45.0f * 3.14159f / 180.0f,
                                      (float)winW / (float)winH,
                                      nearPlane, farPlane);

        shader.use();
        shader.setBool("isClouds", false);
        shader.setMat4("view",       view.m);
        shader.setMat4("projection", projection.m);
        shader.setVec3("lightPos",   0.0f, 0.0f, 0.0f);
        shader.setVec3("lightColor", 1.0f, 1.0f, 0.95f);

        drawStars(shader, sphereLo);
        drawAsteroidBelt(shader, sphereLo, currentTime, simSpeed);

        // Orbit rings
        float orbitRadii[] = {14.0f, 20.0f, 27.0f, 35.0f, 50.0f, 65.0f, 78.0f, 90.0f};
        for (int i = 0; i < 8; i++)
            orbitRing.draw(lineShader, 0.0f, 0.0f, orbitRadii[i],
                           0.22f, 0.25f, 0.35f, 0.6f, view.m, projection.m);
        shader.use();
        shader.setMat4("view",       view.m);
        shader.setMat4("projection", projection.m);
        shader.setVec3("lightPos",   0.0f, 0.0f, 0.0f);
        shader.setVec3("lightColor", 1.0f, 1.0f, 0.95f);

        // Sun
        shader.setBool("isSun",    true);
        shader.setBool("isEarth",  false);
        shader.setBool("isClouds", false);
        shader.setMat4("model", scale(sunSize, sunSize, sunSize).m);
        glBindTexture(GL_TEXTURE_2D, texSun);
        drawLOD(sphereHi, sphereMed, sphereLo, eyeX, eyeY, eyeZ, 0.0f, 0.0f);

        // Sun glow
        {
            float glow = sunSize * 1.4f * (1.0f + 0.05f * sinf(currentTime * 2.0f));
            shader.setMat4("model", scale(glow, glow, glow).m);
            glDepthMask(GL_FALSE);
            sphereLo.draw();
            glDepthMask(GL_TRUE);
        }

        shader.setBool("isSun", false);
        float savedEarthX=0, savedEarthZ=0;
        float savedMarsX=0,  savedMarsZ=0;
        float savedJupX=0,   savedJupZ=0;

        for (int i = 0; i < planetCount; i++)
        {
            float px = planetWorldX[i];
            float pz = planetWorldZ[i];

            if (i == 2) { savedEarthX = px; savedEarthZ = pz; }
            if (i == 3) { savedMarsX  = px; savedMarsZ  = pz; }
            if (i == 4) { savedJupX   = px; savedJupZ   = pz; }

            Mat4 model = multiply(
                translate(px, 0.0f, pz),
                multiply(rotateX(planets[i].tilt),
                multiply(rotateY(planets[i].selfAngle),
                scale(planets[i].size, planets[i].size, planets[i].size))));

            shader.setMat4("model", model.m);
            shader.setBool("isEarth",  i == 2);
            shader.setBool("isClouds", false);
            glBindTexture(GL_TEXTURE_2D, planets[i].texture);
            drawLOD(sphereHi, sphereMed, sphereLo, eyeX, eyeY, eyeZ, px, pz);
            shader.setBool("isEarth", false);

            // Earth clouds
            if (i == 2)
            {
                float cs = planets[i].size * 1.02f;
                Mat4 cm = multiply(translate(px,0,pz),
                           multiply(rotateX(planets[i].tilt),
                           multiply(rotateY(planets[i].selfAngle * 0.7f),
                           scale(cs, cs, cs))));
                shader.setMat4("model", cm.m);
                shader.setBool("isSun", false);
                shader.setBool("isEarth", false);
                shader.setBool("isClouds", true);
                glBindTexture(GL_TEXTURE_2D, texEarthClouds);
                glDepthMask(GL_FALSE);
                drawLOD(sphereHi, sphereMed, sphereLo, eyeX, eyeY, eyeZ, px, pz);
                glDepthMask(GL_TRUE);
                shader.setBool("isClouds", false);
            }

            // Saturn ring gradient
            if (i == 5)
            {
                float PI = 3.14159f;
                float minR = planets[i].size * 1.4f;
                float maxR = planets[i].size * 2.4f;
                shader.setBool("isSun", true);
                shader.setBool("isClouds", false);
                glBindTexture(GL_TEXTURE_2D, texSaturnRing);
                for (int rr = 0; rr < 8; rr++)
                {
                    float t   = (float)rr / 7.0f;
                    float rR  = minR + (maxR - minR) * t;
                    float bri = 1.0f - t * 0.4f;
                    shader.setVec3("lightColor", bri, bri*0.92f, bri*0.75f);
                    for (int s = 0; s < 150; s++)
                    {
                        float sa = 2.0f * PI * s / 150;
                        Mat4 rm = multiply(translate(px + rR*cosf(sa), 0, pz + rR*sinf(sa)),
                                           scale(0.04f, 0.008f, 0.04f));
                        shader.setMat4("model", rm.m);
                        sphereLo.draw();
                    }
                }
                shader.setVec3("lightColor", 1.0f, 1.0f, 0.95f);
                shader.setBool("isSun", false);
            }

            // Uranus ring
            if (i == 6)
            {
                float PI = 3.14159f, rR = planets[i].size * 1.6f;
                shader.setBool("isSun", true);
                shader.setBool("isEarth", false);
                shader.setBool("isClouds", false);
                glBindTexture(GL_TEXTURE_2D, texUranus);
                for (int s = 0; s < 120; s++)
                {
                    float sa = 2.0f * PI * s / 120;
                    Mat4 rm = multiply(translate(px + rR*cosf(sa), rR*sinf(sa), pz),
                                       scale(0.03f, 0.03f, 0.03f));
                    shader.setMat4("model", rm.m);
                    sphereLo.draw();
                }
                shader.setBool("isSun", false);
            }

            // Neptune ring
            if (i == 7)
            {
                float PI = 3.14159f, rR = planets[i].size * 1.5f;
                shader.setBool("isSun", true);
                shader.setBool("isEarth", false);
                shader.setBool("isClouds", false);
                glBindTexture(GL_TEXTURE_2D, texNeptune);
                for (int s = 0; s < 100; s++)
                {
                    float sa = 2.0f * PI * s / 100;
                    Mat4 rm = multiply(translate(px + rR*cosf(sa), 0, pz + rR*sinf(sa)),
                                       scale(0.025f, 0.008f, 0.025f));
                    shader.setMat4("model", rm.m);
                    sphereLo.draw();
                }
                shader.setBool("isSun", false);
            }
        }

        // Earth Moon
        {
            float ma = planets[2].orbitAngle * 13.0f;
            float mx = savedEarthX + 3.5f * cosf(ma);
            float mz = savedEarthZ + 3.5f * sinf(ma);
            shader.setMat4("model", multiply(translate(mx,0,mz), scale(0.27f,0.27f,0.27f)).m);
            shader.setBool("isSun", false);
            shader.setBool("isEarth", false);
            shader.setBool("isClouds", false);
            glBindTexture(GL_TEXTURE_2D, texMoon);
            drawLOD(sphereHi, sphereMed, sphereLo, eyeX, eyeY, eyeZ, mx, mz);
        }

        // Mars moons
        {
            shader.setBool("isSun", false);
            glBindTexture(GL_TEXTURE_2D, texMoon);
            float pa = planets[3].orbitAngle;
            float px1 = savedMarsX + 1.4f*cosf(pa*20); float pz1 = savedMarsZ + 1.4f*sinf(pa*20);
            float px2 = savedMarsX + 2.0f*cosf(pa*10); float pz2 = savedMarsZ + 2.0f*sinf(pa*10);
            shader.setMat4("model", multiply(translate(px1,0,pz1),scale(0.08f,0.08f,0.08f)).m);
            sphereLo.draw();
            shader.setMat4("model", multiply(translate(px2,0,pz2),scale(0.06f,0.06f,0.06f)).m);
            sphereLo.draw();
        }

        // Jupiter moons
        {
            float md[4][2] = {{2.8f,30},{3.6f,18},{4.6f,10},{5.8f,5}};
            float ms[4]    = {0.12f, 0.10f, 0.14f, 0.13f};
            shader.setBool("isSun", false);
            shader.setBool("isEarth", false);
            shader.setBool("isClouds", false);
            glBindTexture(GL_TEXTURE_2D, texMoon);
            for (int m = 0; m < 4; m++)
            {
                float ma = planets[4].orbitAngle * md[m][1];
                float mx = savedJupX + md[m][0]*cosf(ma);
                float mz = savedJupZ + md[m][0]*sinf(ma);
                shader.setMat4("model",
                    multiply(translate(mx,0,mz), scale(ms[m],ms[m],ms[m])).m);
                sphereLo.draw();
            }
        }

        drawComet(shader, sphereLo, cometAngle, texComet);

        // Title bar — FPS + planet info panel
        {
            static float lastT  = 0;
            static int   frameC = 0;
            static float fpsVal = 0;
            frameC++;
            if (currentTime - lastT >= 0.5f)
            {
                fpsVal = frameC / (currentTime - lastT);
                frameC = 0; lastT = currentTime;
            }

            char title[256];
            if (followPlanet >= 0 && followPlanet < 8)
            {
                Planet &p = planets[followPlanet];
                sprintf(title,
                    "3D Solar System | FPS:%.0f | Speed:%.1fx | %s | "
                    ">> %s | %s | Moons:%d | Day:%.2f Earth-days <<",
                    fpsVal, simSpeed, paused ? "PAUSED" : "RUNNING",
                    p.name, p.type, p.moons, p.dayLength);
            }
            else if (spacecraftMode)
            {
                sprintf(title,
                    "3D Solar System | FPS:%.0f | SPACECRAFT | "
                    "Pos:(%.0f,%.0f,%.0f) | Throttle:%.1f | WASD=fly Q/E=up-dn V=exit",
                    fpsVal, scX, scY, scZ, scSpeed);
            }
            else
            {
                sprintf(title,
                    "3D Solar System - Naimur Rashid | FPS:%.0f | Speed:%.1fx | %s | "
                    "Dist:%.0f | [I=controls] [V=spacecraft] [1-8=follow]",
                    fpsVal, simSpeed, paused ? "PAUSED" : "RUNNING", camDistance);
            }
            glfwSetWindowTitle(window, title);

            static bool lastHelp = false;
            if (showHelp && !lastHelp) printHelp();
            lastHelp = showHelp;
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}