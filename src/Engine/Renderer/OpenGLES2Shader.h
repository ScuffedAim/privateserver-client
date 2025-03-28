//================ Copyright (c) 2019, PG, All rights reserved. =================//
//
// Purpose:		OpenGLES2 GLSL implementation of Shader
//
// $NoKeywords: $gles2shader
//===============================================================================//

#ifndef OPENGLES2SHADER_H
#define OPENGLES2SHADER_H

#include "Shader.h"

#ifdef MCENGINE_FEATURE_OPENGLES

class OpenGLES2Shader : public Shader {
   public:
    OpenGLES2Shader(std::string vertexShader, std::string fragmentShader, bool source);
    virtual ~OpenGLES2Shader() { destroy(); }

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

    int getAttribLocation(UString name);

    // ILLEGAL:
    bool isActive();

   private:
    virtual void init();
    virtual void initAsync();
    virtual void destroy();

    bool compile(std::string vertexShader, std::string fragmentShader, bool source);
    int createShaderFromString(std::string shaderSource, int shaderType);
    int createShaderFromFile(std::string fileName, int shaderType);

    std::string m_sVsh, this->sFsh;

    bool m_bSource;
    int m_iVertexShader;
    int m_iFragmentShader;
    int m_iProgram;

    int m_iProgramBackup;
};

#endif

#endif
