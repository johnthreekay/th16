// Stand-in for the DirectX SDK's d3dx9math.h (with d3dx9math.inl) in the
// portable build: the vector and matrix classes with their operators, the
// helpers the SDK defines inline (written as the SDK writes them, since the
// original inlined them into game code), and declarations of the D3DX math
// functions, which port/src/d3dx9_math.cpp implements.
#pragma once

#include <math.h>

#include <d3d9.h>

#define D3DX_PI ((FLOAT)3.141592654f)
#define D3DX_1BYPI ((FLOAT)0.318309886f)
#define D3DXToRadian(degree) ((degree) * (D3DX_PI / 180.0f))
#define D3DXToDegree(radian) ((radian) * (180.0f / D3DX_PI))

struct D3DXVECTOR2
{
    FLOAT x;
    FLOAT y;

    D3DXVECTOR2() = default;
    D3DXVECTOR2(const FLOAT *pf) : x(pf[0]), y(pf[1]) {}
    D3DXVECTOR2(FLOAT fx, FLOAT fy) : x(fx), y(fy) {}

    // MSVC lets the game take the address of a temporary vector
    // (&(a - b), &Float3(x, y, z)); a member operator& does the same in
    // standard C++. The pointer is valid until the end of the full
    // expression, as it was there.
    D3DXVECTOR2 *operator&() { return this; }
    const D3DXVECTOR2 *operator&() const { return this; }

    operator FLOAT *() { return (FLOAT *)&x; }
    operator const FLOAT *() const { return (const FLOAT *)&x; }

    D3DXVECTOR2 &operator+=(const D3DXVECTOR2 &v) { x += v.x; y += v.y; return *this; }
    D3DXVECTOR2 &operator-=(const D3DXVECTOR2 &v) { x -= v.x; y -= v.y; return *this; }
    D3DXVECTOR2 &operator*=(FLOAT f) { x *= f; y *= f; return *this; }
    D3DXVECTOR2 &operator/=(FLOAT f)
    {
        FLOAT inv = 1.0f / f;
        x *= inv;
        y *= inv;
        return *this;
    }

    D3DXVECTOR2 operator+() const { return *this; }
    D3DXVECTOR2 operator-() const { return D3DXVECTOR2(-x, -y); }

    D3DXVECTOR2 operator+(const D3DXVECTOR2 &v) const { return D3DXVECTOR2(x + v.x, y + v.y); }
    D3DXVECTOR2 operator-(const D3DXVECTOR2 &v) const { return D3DXVECTOR2(x - v.x, y - v.y); }
    D3DXVECTOR2 operator*(FLOAT f) const { return D3DXVECTOR2(x * f, y * f); }
    D3DXVECTOR2 operator/(FLOAT f) const
    {
        FLOAT inv = 1.0f / f;
        return D3DXVECTOR2(x * inv, y * inv);
    }
    friend D3DXVECTOR2 operator*(FLOAT f, const D3DXVECTOR2 &v) { return D3DXVECTOR2(f * v.x, f * v.y); }

    BOOL operator==(const D3DXVECTOR2 &v) const { return x == v.x && y == v.y; }
    BOOL operator!=(const D3DXVECTOR2 &v) const { return x != v.x || y != v.y; }
};
typedef D3DXVECTOR2 *LPD3DXVECTOR2;

struct D3DXVECTOR3 : public D3DVECTOR
{
    D3DXVECTOR3() = default;
    D3DXVECTOR3(const FLOAT *pf)
    {
        x = pf[0];
        y = pf[1];
        z = pf[2];
    }
    D3DXVECTOR3(const D3DVECTOR &v)
    {
        x = v.x;
        y = v.y;
        z = v.z;
    }
    D3DXVECTOR3(FLOAT fx, FLOAT fy, FLOAT fz)
    {
        x = fx;
        y = fy;
        z = fz;
    }

    // MSVC lets the game take the address of a temporary vector
    // (&(a - b), &Float3(x, y, z)); a member operator& does the same in
    // standard C++. The pointer is valid until the end of the full
    // expression, as it was there.
    D3DXVECTOR3 *operator&() { return this; }
    const D3DXVECTOR3 *operator&() const { return this; }

    operator FLOAT *() { return (FLOAT *)&x; }
    operator const FLOAT *() const { return (const FLOAT *)&x; }

    D3DXVECTOR3 &operator+=(const D3DXVECTOR3 &v) { x += v.x; y += v.y; z += v.z; return *this; }
    D3DXVECTOR3 &operator-=(const D3DXVECTOR3 &v) { x -= v.x; y -= v.y; z -= v.z; return *this; }
    D3DXVECTOR3 &operator*=(FLOAT f) { x *= f; y *= f; z *= f; return *this; }
    D3DXVECTOR3 &operator/=(FLOAT f)
    {
        FLOAT inv = 1.0f / f;
        x *= inv;
        y *= inv;
        z *= inv;
        return *this;
    }

    D3DXVECTOR3 operator+() const { return *this; }
    D3DXVECTOR3 operator-() const { return D3DXVECTOR3(-x, -y, -z); }

    D3DXVECTOR3 operator+(const D3DXVECTOR3 &v) const { return D3DXVECTOR3(x + v.x, y + v.y, z + v.z); }
    D3DXVECTOR3 operator-(const D3DXVECTOR3 &v) const { return D3DXVECTOR3(x - v.x, y - v.y, z - v.z); }
    D3DXVECTOR3 operator*(FLOAT f) const { return D3DXVECTOR3(x * f, y * f, z * f); }
    D3DXVECTOR3 operator/(FLOAT f) const
    {
        FLOAT inv = 1.0f / f;
        return D3DXVECTOR3(x * inv, y * inv, z * inv);
    }
    friend D3DXVECTOR3 operator*(FLOAT f, const D3DXVECTOR3 &v) { return D3DXVECTOR3(f * v.x, f * v.y, f * v.z); }

    BOOL operator==(const D3DXVECTOR3 &v) const { return x == v.x && y == v.y && z == v.z; }
    BOOL operator!=(const D3DXVECTOR3 &v) const { return x != v.x || y != v.y || z != v.z; }
};
typedef D3DXVECTOR3 *LPD3DXVECTOR3;

// A D3DXVECTOR3 for members of anonymous structs (an MSVC extension that
// GCC only allows for members without constructors, which D3DXVECTOR3 has;
// used by Bomb.h's BombReimuAOrb). The same layout with no constructors:
// it converts to and from D3DXVECTOR3, its address is a D3DXVECTOR3 *, and
// it has D3DXVECTOR3's arithmetic.
struct PortAnonVec3 : public D3DVECTOR
{
    PortAnonVec3 &operator=(const D3DVECTOR &v)
    {
        x = v.x;
        y = v.y;
        z = v.z;
        return *this;
    }
    D3DXVECTOR3 *operator&() { return (D3DXVECTOR3 *)this; }
    const D3DXVECTOR3 *operator&() const { return (const D3DXVECTOR3 *)this; }
    operator D3DXVECTOR3 &() { return *(D3DXVECTOR3 *)this; }
    operator const D3DXVECTOR3 &() const { return *(const D3DXVECTOR3 *)this; }
    D3DXVECTOR3 &vec() { return *(D3DXVECTOR3 *)this; }
    const D3DXVECTOR3 &vec() const { return *(const D3DXVECTOR3 *)this; }

    D3DXVECTOR3 &operator+=(const D3DXVECTOR3 &v) { return vec() += v; }
    D3DXVECTOR3 &operator-=(const D3DXVECTOR3 &v) { return vec() -= v; }
    D3DXVECTOR3 &operator*=(FLOAT f) { return vec() *= f; }
    D3DXVECTOR3 &operator/=(FLOAT f) { return vec() /= f; }
    D3DXVECTOR3 operator-() const { return -vec(); }
    D3DXVECTOR3 operator+(const D3DXVECTOR3 &v) const { return vec() + v; }
    D3DXVECTOR3 operator-(const D3DXVECTOR3 &v) const { return vec() - v; }
    D3DXVECTOR3 operator*(FLOAT f) const { return vec() * f; }
    D3DXVECTOR3 operator/(FLOAT f) const { return vec() / f; }
};

struct D3DXVECTOR4
{
    FLOAT x;
    FLOAT y;
    FLOAT z;
    FLOAT w;

    D3DXVECTOR4() = default;
    D3DXVECTOR4(const FLOAT *pf) : x(pf[0]), y(pf[1]), z(pf[2]), w(pf[3]) {}
    D3DXVECTOR4(const D3DVECTOR &v, FLOAT f) : x(v.x), y(v.y), z(v.z), w(f) {}
    D3DXVECTOR4(FLOAT fx, FLOAT fy, FLOAT fz, FLOAT fw) : x(fx), y(fy), z(fz), w(fw) {}

    // MSVC lets the game take the address of a temporary vector
    // (&(a - b), &Float3(x, y, z)); a member operator& does the same in
    // standard C++. The pointer is valid until the end of the full
    // expression, as it was there.
    D3DXVECTOR4 *operator&() { return this; }
    const D3DXVECTOR4 *operator&() const { return this; }

    operator FLOAT *() { return (FLOAT *)&x; }
    operator const FLOAT *() const { return (const FLOAT *)&x; }

    D3DXVECTOR4 &operator+=(const D3DXVECTOR4 &v) { x += v.x; y += v.y; z += v.z; w += v.w; return *this; }
    D3DXVECTOR4 &operator-=(const D3DXVECTOR4 &v) { x -= v.x; y -= v.y; z -= v.z; w -= v.w; return *this; }
    D3DXVECTOR4 &operator*=(FLOAT f) { x *= f; y *= f; z *= f; w *= f; return *this; }
    D3DXVECTOR4 &operator/=(FLOAT f)
    {
        FLOAT inv = 1.0f / f;
        x *= inv;
        y *= inv;
        z *= inv;
        w *= inv;
        return *this;
    }

    D3DXVECTOR4 operator+() const { return *this; }
    D3DXVECTOR4 operator-() const { return D3DXVECTOR4(-x, -y, -z, -w); }

    D3DXVECTOR4 operator+(const D3DXVECTOR4 &v) const { return D3DXVECTOR4(x + v.x, y + v.y, z + v.z, w + v.w); }
    D3DXVECTOR4 operator-(const D3DXVECTOR4 &v) const { return D3DXVECTOR4(x - v.x, y - v.y, z - v.z, w - v.w); }
    D3DXVECTOR4 operator*(FLOAT f) const { return D3DXVECTOR4(x * f, y * f, z * f, w * f); }
    D3DXVECTOR4 operator/(FLOAT f) const
    {
        FLOAT inv = 1.0f / f;
        return D3DXVECTOR4(x * inv, y * inv, z * inv, w * inv);
    }
    friend D3DXVECTOR4 operator*(FLOAT f, const D3DXVECTOR4 &v)
    {
        return D3DXVECTOR4(f * v.x, f * v.y, f * v.z, f * v.w);
    }

    BOOL operator==(const D3DXVECTOR4 &v) const { return x == v.x && y == v.y && z == v.z && w == v.w; }
    BOOL operator!=(const D3DXVECTOR4 &v) const { return x != v.x || y != v.y || z != v.z || w != v.w; }
};
typedef D3DXVECTOR4 *LPD3DXVECTOR4;

struct D3DXMATRIX;
extern "C" D3DXMATRIX *D3DXMatrixMultiply(D3DXMATRIX *pOut, const D3DXMATRIX *pM1, const D3DXMATRIX *pM2);

struct D3DXMATRIX : public D3DMATRIX
{
    D3DXMATRIX() = default;
    D3DXMATRIX(const FLOAT *pf) { memcpy(&_11, pf, sizeof(D3DXMATRIX)); }
    D3DXMATRIX(const D3DMATRIX &mat) { memcpy(&_11, &mat, sizeof(D3DXMATRIX)); }
    D3DXMATRIX(FLOAT f11, FLOAT f12, FLOAT f13, FLOAT f14, FLOAT f21, FLOAT f22, FLOAT f23, FLOAT f24, FLOAT f31,
               FLOAT f32, FLOAT f33, FLOAT f34, FLOAT f41, FLOAT f42, FLOAT f43, FLOAT f44)
    {
        _11 = f11; _12 = f12; _13 = f13; _14 = f14;
        _21 = f21; _22 = f22; _23 = f23; _24 = f24;
        _31 = f31; _32 = f32; _33 = f33; _34 = f34;
        _41 = f41; _42 = f42; _43 = f43; _44 = f44;
    }

    FLOAT &operator()(UINT row, UINT col) { return m[row][col]; }
    FLOAT operator()(UINT row, UINT col) const { return m[row][col]; }

    // MSVC lets the game take the address of a temporary vector
    // (&(a - b), &Float3(x, y, z)); a member operator& does the same in
    // standard C++. The pointer is valid until the end of the full
    // expression, as it was there.
    D3DXMATRIX *operator&() { return this; }
    const D3DXMATRIX *operator&() const { return this; }

    operator FLOAT *() { return (FLOAT *)&_11; }
    operator const FLOAT *() const { return (const FLOAT *)&_11; }

    D3DXMATRIX &operator*=(const D3DXMATRIX &mat)
    {
        D3DXMatrixMultiply(this, this, &mat);
        return *this;
    }
    D3DXMATRIX &operator+=(const D3DXMATRIX &mat)
    {
        for (int i = 0; i < 16; i++)
            (&_11)[i] += (&mat._11)[i];
        return *this;
    }
    D3DXMATRIX &operator-=(const D3DXMATRIX &mat)
    {
        for (int i = 0; i < 16; i++)
            (&_11)[i] -= (&mat._11)[i];
        return *this;
    }
    D3DXMATRIX &operator*=(FLOAT f)
    {
        for (int i = 0; i < 16; i++)
            (&_11)[i] *= f;
        return *this;
    }
    D3DXMATRIX &operator/=(FLOAT f)
    {
        FLOAT inv = 1.0f / f;
        for (int i = 0; i < 16; i++)
            (&_11)[i] *= inv;
        return *this;
    }

    D3DXMATRIX operator+() const { return *this; }
    D3DXMATRIX operator-() const
    {
        D3DXMATRIX r;
        for (int i = 0; i < 16; i++)
            (&r._11)[i] = -(&_11)[i];
        return r;
    }

    D3DXMATRIX operator*(const D3DXMATRIX &mat) const
    {
        D3DXMATRIX r;
        D3DXMatrixMultiply(&r, this, &mat);
        return r;
    }
    D3DXMATRIX operator+(const D3DXMATRIX &mat) const
    {
        D3DXMATRIX r = *this;
        return r += mat;
    }
    D3DXMATRIX operator-(const D3DXMATRIX &mat) const
    {
        D3DXMATRIX r = *this;
        return r -= mat;
    }
    D3DXMATRIX operator*(FLOAT f) const
    {
        D3DXMATRIX r = *this;
        return r *= f;
    }
    D3DXMATRIX operator/(FLOAT f) const
    {
        D3DXMATRIX r = *this;
        return r /= f;
    }
    friend D3DXMATRIX operator*(FLOAT f, const D3DXMATRIX &mat) { return mat * f; }

    BOOL operator==(const D3DXMATRIX &mat) const { return memcmp(this, &mat, sizeof(D3DXMATRIX)) == 0; }
    BOOL operator!=(const D3DXMATRIX &mat) const { return memcmp(this, &mat, sizeof(D3DXMATRIX)) != 0; }
};
typedef D3DXMATRIX *LPD3DXMATRIX;

// d3dx9math.inl: inline in the SDK, so in the original too.

inline FLOAT D3DXVec2Length(const D3DXVECTOR2 *pV)
{
    return sqrtf(pV->x * pV->x + pV->y * pV->y);
}

inline FLOAT D3DXVec2LengthSq(const D3DXVECTOR2 *pV)
{
    return pV->x * pV->x + pV->y * pV->y;
}

inline FLOAT D3DXVec2Dot(const D3DXVECTOR2 *pV1, const D3DXVECTOR2 *pV2)
{
    return pV1->x * pV2->x + pV1->y * pV2->y;
}

inline D3DXVECTOR2 *D3DXVec2Add(D3DXVECTOR2 *pOut, const D3DXVECTOR2 *pV1, const D3DXVECTOR2 *pV2)
{
    pOut->x = pV1->x + pV2->x;
    pOut->y = pV1->y + pV2->y;
    return pOut;
}

inline D3DXVECTOR2 *D3DXVec2Subtract(D3DXVECTOR2 *pOut, const D3DXVECTOR2 *pV1, const D3DXVECTOR2 *pV2)
{
    pOut->x = pV1->x - pV2->x;
    pOut->y = pV1->y - pV2->y;
    return pOut;
}

inline D3DXVECTOR2 *D3DXVec2Scale(D3DXVECTOR2 *pOut, const D3DXVECTOR2 *pV, FLOAT s)
{
    pOut->x = pV->x * s;
    pOut->y = pV->y * s;
    return pOut;
}

inline FLOAT D3DXVec3Length(const D3DXVECTOR3 *pV)
{
    return sqrtf(pV->x * pV->x + pV->y * pV->y + pV->z * pV->z);
}

inline FLOAT D3DXVec3LengthSq(const D3DXVECTOR3 *pV)
{
    return pV->x * pV->x + pV->y * pV->y + pV->z * pV->z;
}

inline FLOAT D3DXVec3Dot(const D3DXVECTOR3 *pV1, const D3DXVECTOR3 *pV2)
{
    return pV1->x * pV2->x + pV1->y * pV2->y + pV1->z * pV2->z;
}

inline D3DXVECTOR3 *D3DXVec3Cross(D3DXVECTOR3 *pOut, const D3DXVECTOR3 *pV1, const D3DXVECTOR3 *pV2)
{
    D3DXVECTOR3 v;
    v.x = pV1->y * pV2->z - pV1->z * pV2->y;
    v.y = pV1->z * pV2->x - pV1->x * pV2->z;
    v.z = pV1->x * pV2->y - pV1->y * pV2->x;
    *pOut = v;
    return pOut;
}

inline D3DXVECTOR3 *D3DXVec3Add(D3DXVECTOR3 *pOut, const D3DXVECTOR3 *pV1, const D3DXVECTOR3 *pV2)
{
    pOut->x = pV1->x + pV2->x;
    pOut->y = pV1->y + pV2->y;
    pOut->z = pV1->z + pV2->z;
    return pOut;
}

inline D3DXVECTOR3 *D3DXVec3Subtract(D3DXVECTOR3 *pOut, const D3DXVECTOR3 *pV1, const D3DXVECTOR3 *pV2)
{
    pOut->x = pV1->x - pV2->x;
    pOut->y = pV1->y - pV2->y;
    pOut->z = pV1->z - pV2->z;
    return pOut;
}

inline D3DXVECTOR3 *D3DXVec3Scale(D3DXVECTOR3 *pOut, const D3DXVECTOR3 *pV, FLOAT s)
{
    pOut->x = pV->x * s;
    pOut->y = pV->y * s;
    pOut->z = pV->z * s;
    return pOut;
}

inline D3DXVECTOR3 *D3DXVec3Lerp(D3DXVECTOR3 *pOut, const D3DXVECTOR3 *pV1, const D3DXVECTOR3 *pV2, FLOAT s)
{
    pOut->x = pV1->x + s * (pV2->x - pV1->x);
    pOut->y = pV1->y + s * (pV2->y - pV1->y);
    pOut->z = pV1->z + s * (pV2->z - pV1->z);
    return pOut;
}

inline D3DXMATRIX *D3DXMatrixIdentity(D3DXMATRIX *pOut)
{
    pOut->m[0][1] = pOut->m[0][2] = pOut->m[0][3] = pOut->m[1][0] = pOut->m[1][2] = pOut->m[1][3] =
        pOut->m[2][0] = pOut->m[2][1] = pOut->m[2][3] = pOut->m[3][0] = pOut->m[3][1] = pOut->m[3][2] = 0.0f;
    pOut->m[0][0] = pOut->m[1][1] = pOut->m[2][2] = pOut->m[3][3] = 1.0f;
    return pOut;
}

inline BOOL D3DXMatrixIsIdentity(const D3DXMATRIX *pM)
{
    return pM->m[0][0] == 1.0f && pM->m[0][1] == 0.0f && pM->m[0][2] == 0.0f && pM->m[0][3] == 0.0f &&
           pM->m[1][0] == 0.0f && pM->m[1][1] == 1.0f && pM->m[1][2] == 0.0f && pM->m[1][3] == 0.0f &&
           pM->m[2][0] == 0.0f && pM->m[2][1] == 0.0f && pM->m[2][2] == 1.0f && pM->m[2][3] == 0.0f &&
           pM->m[3][0] == 0.0f && pM->m[3][1] == 0.0f && pM->m[3][2] == 0.0f && pM->m[3][3] == 1.0f;
}

// The D3DX DLL's functions (port/src/d3dx9_math.cpp).
extern "C" {
D3DXVECTOR2 *D3DXVec2Normalize(D3DXVECTOR2 *pOut, const D3DXVECTOR2 *pV);
D3DXVECTOR3 *D3DXVec3Normalize(D3DXVECTOR3 *pOut, const D3DXVECTOR3 *pV);
D3DXVECTOR4 *D3DXVec3Transform(D3DXVECTOR4 *pOut, const D3DXVECTOR3 *pV, const D3DXMATRIX *pM);
D3DXVECTOR3 *D3DXVec3TransformCoord(D3DXVECTOR3 *pOut, const D3DXVECTOR3 *pV, const D3DXMATRIX *pM);
D3DXVECTOR3 *D3DXVec3TransformNormal(D3DXVECTOR3 *pOut, const D3DXVECTOR3 *pV, const D3DXMATRIX *pM);
D3DXVECTOR3 *D3DXVec3Project(D3DXVECTOR3 *pOut, const D3DXVECTOR3 *pV, const D3DVIEWPORT9 *pViewport,
                             const D3DXMATRIX *pProjection, const D3DXMATRIX *pView, const D3DXMATRIX *pWorld);
D3DXVECTOR3 *D3DXVec3ProjectArray(D3DXVECTOR3 *pOut, UINT OutStride, const D3DXVECTOR3 *pV, UINT VStride,
                                  const D3DVIEWPORT9 *pViewport, const D3DXMATRIX *pProjection,
                                  const D3DXMATRIX *pView, const D3DXMATRIX *pWorld, UINT n);
D3DXMATRIX *D3DXMatrixTranspose(D3DXMATRIX *pOut, const D3DXMATRIX *pM);
D3DXMATRIX *D3DXMatrixInverse(D3DXMATRIX *pOut, FLOAT *pDeterminant, const D3DXMATRIX *pM);
D3DXMATRIX *D3DXMatrixScaling(D3DXMATRIX *pOut, FLOAT sx, FLOAT sy, FLOAT sz);
D3DXMATRIX *D3DXMatrixTranslation(D3DXMATRIX *pOut, FLOAT x, FLOAT y, FLOAT z);
D3DXMATRIX *D3DXMatrixRotationX(D3DXMATRIX *pOut, FLOAT Angle);
D3DXMATRIX *D3DXMatrixRotationY(D3DXMATRIX *pOut, FLOAT Angle);
D3DXMATRIX *D3DXMatrixRotationZ(D3DXMATRIX *pOut, FLOAT Angle);
D3DXMATRIX *D3DXMatrixLookAtLH(D3DXMATRIX *pOut, const D3DXVECTOR3 *pEye, const D3DXVECTOR3 *pAt,
                               const D3DXVECTOR3 *pUp);
D3DXMATRIX *D3DXMatrixPerspectiveFovLH(D3DXMATRIX *pOut, FLOAT fovy, FLOAT Aspect, FLOAT zn, FLOAT zf);
D3DXMATRIX *D3DXMatrixOrthoOffCenterLH(D3DXMATRIX *pOut, FLOAT l, FLOAT r, FLOAT b, FLOAT t, FLOAT zn,
                                       FLOAT zf);
}

// The SDK's d3dx9math.h includes d3dx9.h, so code that includes only the
// math header also sees the texture functions.
#include <d3dx9tex.h>
