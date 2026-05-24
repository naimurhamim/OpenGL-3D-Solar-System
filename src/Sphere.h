#pragma once
#include <glad/glad.h>
#include <vector>
#include <math.h>

class Sphere
{
public:
    unsigned int VAO, VBO, EBO;
    unsigned int indexCount;

    Sphere(int stackCount = 36, int sectorCount = 36)
    {
        std::vector<float>        vertices;
        std::vector<unsigned int> indices;

        float PI = 3.14159265f;

        for(int i = 0; i <= stackCount; i++)
        {
            float stackAngle = PI/2 - PI * i / stackCount;
            float xy = cosf(stackAngle);
            float z  = sinf(stackAngle);

            for(int j = 0; j <= sectorCount; j++)
            {
                float sectorAngle = 2 * PI * j / sectorCount;

                float x = xy * cosf(sectorAngle);
                float y = xy * sinf(sectorAngle);

                // Position
                vertices.push_back(x);
                vertices.push_back(y);
                vertices.push_back(z);

                // TexCoord
                vertices.push_back((float)j / sectorCount);
                vertices.push_back((float)i / stackCount);

                // Normal
                vertices.push_back(x);
                vertices.push_back(y);
                vertices.push_back(z);
            }
        }

        for(int i = 0; i < stackCount; i++)
        {
            int k1 = i * (sectorCount + 1);
            int k2 = k1 + sectorCount + 1;

            for(int j = 0; j < sectorCount; j++, k1++, k2++)
            {
                if(i != 0) {
                    indices.push_back(k1);
                    indices.push_back(k2);
                    indices.push_back(k1+1);
                }
                if(i != stackCount-1) {
                    indices.push_back(k1+1);
                    indices.push_back(k2);
                    indices.push_back(k2+1);
                }
            }
        }

        indexCount = indices.size();

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size()*sizeof(float), vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size()*sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

        // Position
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8*sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        // TexCoord
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8*sizeof(float), (void*)(3*sizeof(float)));
        glEnableVertexAttribArray(1);

        // Normal
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8*sizeof(float), (void*)(5*sizeof(float)));
        glEnableVertexAttribArray(2);

        glBindVertexArray(0);
    }

    void draw()
    {
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
};