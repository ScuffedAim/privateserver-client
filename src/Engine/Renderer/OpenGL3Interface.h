//================ Copyright (c) 2017, PG, All rights reserved. =================//
//
// Purpose:		raw opengl 3.x graphics interface
//
// $NoKeywords: $gl3i
//===============================================================================//

#ifndef OPENGL3INTERFACE_H
#define OPENGL3INTERFACE_H

#include "cbase.h"

#ifdef MCENGINE_FEATURE_OPENGL

class OpenGLShader;

class OpenGL3Interface : public Graphics {
   public:
    OpenGL3Interface();
    virtual ~OpenGL3Interface();

    // scene
    virtual void beginScene();
    virtual void endScene();

    // depth buffer
    virtual void clearDepthBuffer();

    // color
    virtual void setColor(Color color);
    virtual void setAlpha(float alpha);

    // 2d primitive drawing
    virtual void drawPixels(int x, int y, int width, int height, Graphics::DRAWPIXELS_TYPE type, const void *pixels);
    virtual void drawPixel(int x, int y);
    virtual void drawLine(int x1, int y1, int x2, int y2);
    virtual void drawLine(Vector2 pos1, Vector2 pos2);
    virtual void drawRect(int x, int y, int width, int height);
    virtual void drawRect(int x, int y, int width, int height, Color top, Color right, Color bottom, Color left);

    virtual void fillRect(int x, int y, int width, int height);
    virtual void fillRoundedRect(int x, int y, int width, int height, int radius);
    virtual void fillGradient(int x, int y, int width, int height, Color topLeftColor, Color topRightColor,
                              Color bottomLeftColor, Color bottomRightColor);

    virtual void drawQuad(int x, int y, int width, int height);
    virtual void drawQuad(Vector2 topLeft, Vector2 topRight, Vector2 bottomRight, Vector2 bottomLeft,
                          Color topLeftColor, Color topRightColor, Color bottomRightColor, Color bottomLeftColor);

    // 2d resource drawing
    virtual void drawImage(Image *image);
    virtual void drawString(McFont *font, UString text);

    // 3d type drawing
    virtual void drawVAO(VertexArrayObject *vao);

    // DEPRECATED: 2d clipping
    virtual void setClipRect(McRect clipRect);
    virtual void pushClipRect(McRect clipRect);
    virtual void popClipRect();

    // stencil
    virtual void pushStencil();
    virtual void fillStencil(bool inside);
    virtual void popStencil();

    // renderer settings
    virtual void setClipping(bool enabled);
    virtual void setAlphaTesting(bool enabled);
    virtual void setAlphaTestFunc(COMPARE_FUNC alphaFunc, float ref);
    virtual void setBlending(bool enabled);
    virtual void setBlendMode(BLEND_MODE blendMode);
    virtual void setDepthBuffer(bool enabled);
    virtual void setCulling(bool culling);
    virtual void setAntialiasing(bool aa);
    virtual void setWireframe(bool enabled);

    // renderer actions
    virtual void flush();
    virtual std::vector<unsigned char> getScreenshot();

    // renderer info
    virtual Vector2 getResolution() const { return this->vResolution; }
    virtual UString getVendor();
    virtual UString getModel();
    virtual UString getVersion();
    virtual int getVRAMTotal();
    virtual int getVRAMRemaining();

    // callbacks
    virtual void onResolutionChange(Vector2 newResolution);

    // factory
    virtual Image *createImage(std::string filePath, bool mipmapped);
    virtual Image *createImage(int width, int height, bool mipmapped);
    virtual RenderTarget *createRenderTarget(int x, int y, int width, int height,
                                             Graphics::MULTISAMPLE_TYPE multiSampleType);
    virtual Shader *createShaderFromFile(std::string vertexShaderFilePath, std::string fragmentShaderFilePath);
    virtual Shader *createShaderFromSource(std::string vertexShader, std::string fragmentShader);
    virtual VertexArrayObject *createVertexArrayObject(Graphics::PRIMITIVE primitive, Graphics::USAGE_TYPE usage,
                                                       bool keepInSystemMemory);

    // ILLEGAL:
    inline const int getShaderGenericAttribPosition() const { return this->iShaderTexturedGenericAttribPosition; }
    inline const int getShaderGenericAttribUV() const { return this->iShaderTexturedGenericAttribUV; }

   protected:
    virtual void init();
    virtual void onTransformUpdate(Matrix4 &projectionMatrix, Matrix4 &worldMatrix);

   private:
    void handleGLErrors();

    static int primitiveToOpenGL(Graphics::PRIMITIVE primitive);
    static int compareFuncToOpenGL(Graphics::COMPARE_FUNC compareFunc);

    // renderer
    bool bInScene;
    Vector2 vResolution;
    Matrix4 projectionMatrix;
    Matrix4 worldMatrix;
    Matrix4 MP;

    OpenGLShader *shaderTexturedGeneric;
    int iShaderTexturedGenericAttribPosition;
    int iShaderTexturedGenericAttribUV;
    int iShaderTexturedGenericAttribCol;
    bool bShaderTexturedGenericIsTextureEnabled;

    unsigned int iVA;
    unsigned int iVBOVertices;
    unsigned int iVBOTexcoords;
    unsigned int iVBOTexcolors;

    // persistent vars
    Color color;

    // clipping
    std::stack<McRect> clipRectStack;
};

#endif

#endif
