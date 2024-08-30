#pragma once
#include <list>

#include "cbase.h"

class Camera;
class ConVar;
class Image;
class Shader;
class VertexArrayObject;

class ModFPoSu {
   public:
    static constexpr const float SIZEDIV3D = 1.0f / 512.0f;  // 1.0f / (float)GameRules::OSU_COORD_WIDTH

   private:
    static constexpr const int SUBDIVISIONS = 4;

   public:
    ModFPoSu();
    ~ModFPoSu();

    void draw(Graphics *g);
    void update();

    void onKeyDown(KeyboardEvent &key);
    void onKeyUp(KeyboardEvent &key);

    inline const Camera *getCamera() const { return m_camera; }

    inline float getEdgeDistance() const { return m_fEdgeDistance; }
    inline bool isCrosshairIntersectingScreen() const { return m_bCrosshairIntersectsScreen; }
    float get3DPlayfieldScale() const;

    inline Shader *getHitcircleShader() const { return m_hitcircleShader; }

   private:
    void handleZoomedChange();
    void noclipMove();

    void setMousePosCompensated(Vector2 newMousePos);
    Vector2 intersectRayMesh(Vector3 pos, Vector3 dir);
    Vector3 calculateUnProjectedVector(Vector2 pos);

    void makePlayfield();
    void makeBackgroundCube();

    void onCurvedChange(UString oldValue, UString newValue);
    void onDistanceChange(UString oldValue, UString newValue);
    void onNoclipChange(UString oldValue, UString newValue);

   private:
    struct VertexPair {
        Vector3 a;
        Vector3 b;
        float textureCoordinate;
        Vector3 normal;

        VertexPair(Vector3 a, Vector3 b, float tc) : a(a), b(b), textureCoordinate(tc) { ; }
    };

   private:
    static float subdivide(std::list<VertexPair> &meshList, const std::list<VertexPair>::iterator &begin,
                           const std::list<VertexPair>::iterator &end, int n, float edgeDistance);
    static Vector3 normalFromTriangle(Vector3 p1, Vector3 p2, Vector3 p3);

   private:
    VertexArrayObject *m_vao;
    VertexArrayObject *m_vaoCube;

    std::list<VertexPair> m_meshList;
    float m_fCircumLength;

    Matrix4 m_modelMatrix;
    Camera *m_camera;
    Vector3 m_vPrevNoclipCameraPos;
    bool m_bKeyLeftDown;
    bool m_bKeyUpDown;
    bool m_bKeyRightDown;
    bool m_bKeyDownDown;
    bool m_bKeySpaceDown;
    bool m_bKeySpaceUpDown;
    Vector3 m_vVelocity;
    bool m_bZoomKeyDown;
    bool m_bZoomed;
    float m_fZoomFOVAnimPercent;

    float m_fEdgeDistance;
    bool m_bCrosshairIntersectsScreen;

    Shader *m_hitcircleShader;
};
