//================ Copyright (c) 2016, PG, All rights reserved. =================//
//
// Purpose:		OpenGL GLSL implementation of Shader
//
// $NoKeywords: $glshader
//===============================================================================//

#ifndef OPENGLSHADER_H
#define OPENGLSHADER_H

#include "Shader.h"

#ifdef MCENGINE_FEATURE_OPENGL

class OpenGLShader : public Shader {
   public:
    OpenGLShader(std::string vertexShader, std::string fragmentShader, bool source);
    virtual ~OpenGLShader() { this->destroy(); }

    virtual void enable();
    virtual void disable();

    virtual void setUniform1f(UString name, float value);
    virtual void setUniform1fv(UString name, int count, float *values);
    virtual void setUniform1i(UString name, int value);
    virtual void setUniform2f(UString name, float x, float y);
    virtual void setUniform2fv(UString name, int count, float *vectors);
    virtual void setUniform3f(UString name, float x, float y, float z);
    virtual void setUniform3fv(UString name, int count, float *vectors);
    virtual void setUniform4f(UString name, float x, float y, float z, float w);
    virtual void setUniformMatrix4fv(UString name, Matrix4 &matrix);
    virtual void setUniformMatrix4fv(UString name, float *v);

    // ILLEGAL:
    int getAttribLocation(UString name);
    int getAndCacheUniformLocation(const UString &name);

   private:
    virtual void init();
    virtual void initAsync();
    virtual void destroy();

    bool compile(std::string vertexShader, std::string fragmentShader, bool source);
    int createShaderFromString(std::string shaderSource, int shaderType);
    int createShaderFromFile(std::string fileName, int shaderType);

    std::string sVsh, sFsh;

    bool bSource;
    int iVertexShader;
    int iFragmentShader;
    int iProgram;

    int iProgramBackup;

    std::unordered_map<std::string, int> uniformLocationCache;
    std::string sTempStringBuffer;
};

#endif

#endif
