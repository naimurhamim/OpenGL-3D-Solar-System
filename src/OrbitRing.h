#pragma once
#include <glad/glad.h>
#include <math.h>
#include "Shader.h"

// Draws a circle using GL_LINE_LOOP — clean thin line, no dots
class OrbitRing
{
public:
    unsigned int VAO, VBO;
    int segments;

    OrbitRing(int segs = 360)
    {
        segments = segs;
        float PI = 3.14159f;

        float *verts = new float[segments * 3];
        for (int i = 0; i < segments; i++)
        {
            float a       = 2.0f * PI * i / segments;
            verts[i*3]    = cosf(a);  // x
            verts[i*3+1]  = 0.0f;    // y
            verts[i*3+2]  = sinf(a);  // z
        }

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, segments * 3 * sizeof(float),
                     verts, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                              3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);

        delete[] verts;
    }

    // Draw ring at (cx, cz) with given radius
    void draw(Shader &lineShader, float cx, float cz, float radius,
              float r, float g, float b, float a,
              const float* view, const float* proj)
    {
        lineShader.use();
        lineShader.setMat4("view",       view);
        lineShader.setMat4("projection", proj);

        // Build scale+translate model in line shader
        // We scale the unit circle by radius and translate to (cx, 0, cz)
        float model[16] = {
            radius, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, radius, 0,
            cx, 0, cz, 1
        };
        glUniformMatrix4fv(glGetUniformLocation(lineShader.ID, "model"),
                           1, GL_FALSE, model);
        glUniform4f(glGetUniformLocation(lineShader.ID, "lineColor"),
                    r, g, b, a);

        glBindVertexArray(VAO);
        glDrawArrays(GL_LINE_LOOP, 0, segments);
        glBindVertexArray(0);
    }
};