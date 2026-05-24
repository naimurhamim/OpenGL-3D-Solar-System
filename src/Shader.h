#pragma once
#include <glad/glad.h>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

class Shader
{
public:
    unsigned int ID;

    Shader(const char* vertexPath, const char* fragmentPath)
    {
        std::string vertexCode, fragmentCode;
        std::ifstream vFile(vertexPath), fFile(fragmentPath);

        std::stringstream vStream, fStream;
        vStream << vFile.rdbuf();
        fStream << fFile.rdbuf();

        vertexCode   = vStream.str();
        fragmentCode = fStream.str();

        const char* vCode = vertexCode.c_str();
        const char* fCode = fragmentCode.c_str();

        unsigned int vertex, fragment;

        vertex = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex, 1, &vCode, NULL);
        glCompileShader(vertex);
        checkErrors(vertex, "VERTEX");

        fragment = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment, 1, &fCode, NULL);
        glCompileShader(fragment);
        checkErrors(fragment, "FRAGMENT");

        ID = glCreateProgram();
        glAttachShader(ID, vertex);
        glAttachShader(ID, fragment);
        glLinkProgram(ID);
        checkErrors(ID, "PROGRAM");

        glDeleteShader(vertex);
        glDeleteShader(fragment);
    }

    void use() { glUseProgram(ID); }

    void setBool(const std::string& name, bool value) {
        glUniform1i(glGetUniformLocation(ID, name.c_str()), (int)value);
    }
    void setInt(const std::string& name, int value) {
        glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
    }
    void setFloat(const std::string& name, float value) {
        glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
    }
    void setVec3(const std::string& name, float x, float y, float z) {
        glUniform3f(glGetUniformLocation(ID, name.c_str()), x, y, z);
    }
    void setMat4(const std::string& name, const float* value) {
        glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, value);
    }

private:
    void checkErrors(unsigned int shader, const char* type)
    {
        int success;
        char log[1024];
        if(std::string(type) != "PROGRAM") {
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if(!success) {
                glGetShaderInfoLog(shader, 1024, NULL, log);
                std::cout << "SHADER ERROR [" << type << "]\n" << log << "\n";
            }
        } else {
            glGetProgramiv(shader, GL_LINK_STATUS, &success);
            if(!success) {
                glGetProgramInfoLog(shader, 1024, NULL, log);
                std::cout << "PROGRAM LINK ERROR\n" << log << "\n";
            }
        }
    }
};