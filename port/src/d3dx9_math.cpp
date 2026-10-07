// The D3DX math functions the game calls (d3dx9_*.dll in the original).
// Complete implementations with the documented D3DX formulas, written the
// way Wine's d3dx9 writes them. D3DX itself used SSE code paths, so the last
// bit of a result can differ from the original; the game uses these for
// drawing (matrices, projection) and for a few directions (Vec2/Vec3
// Normalize), see NOTES.md.
#include <math.h>

#include <d3dx9math.h>

extern "C" {

D3DXVECTOR2 *D3DXVec2Normalize(D3DXVECTOR2 *pOut, const D3DXVECTOR2 *pV)
{
    FLOAT norm = D3DXVec2Length(pV);
    if (norm == 0.0f)
    {
        pOut->x = 0.0f;
        pOut->y = 0.0f;
    }
    else
    {
        pOut->x = pV->x / norm;
        pOut->y = pV->y / norm;
    }
    return pOut;
}

D3DXVECTOR3 *D3DXVec3Normalize(D3DXVECTOR3 *pOut, const D3DXVECTOR3 *pV)
{
    FLOAT norm = D3DXVec3Length(pV);
    if (norm == 0.0f)
    {
        pOut->x = 0.0f;
        pOut->y = 0.0f;
        pOut->z = 0.0f;
    }
    else
    {
        pOut->x = pV->x / norm;
        pOut->y = pV->y / norm;
        pOut->z = pV->z / norm;
    }
    return pOut;
}

D3DXVECTOR4 *D3DXVec3Transform(D3DXVECTOR4 *pOut, const D3DXVECTOR3 *pV, const D3DXMATRIX *pM)
{
    D3DXVECTOR4 out;
    out.x = pM->m[0][0] * pV->x + pM->m[1][0] * pV->y + pM->m[2][0] * pV->z + pM->m[3][0];
    out.y = pM->m[0][1] * pV->x + pM->m[1][1] * pV->y + pM->m[2][1] * pV->z + pM->m[3][1];
    out.z = pM->m[0][2] * pV->x + pM->m[1][2] * pV->y + pM->m[2][2] * pV->z + pM->m[3][2];
    out.w = pM->m[0][3] * pV->x + pM->m[1][3] * pV->y + pM->m[2][3] * pV->z + pM->m[3][3];
    *pOut = out;
    return pOut;
}

D3DXVECTOR3 *D3DXVec3TransformCoord(D3DXVECTOR3 *pOut, const D3DXVECTOR3 *pV, const D3DXMATRIX *pM)
{
    D3DXVECTOR3 out;
    FLOAT norm = pM->m[0][3] * pV->x + pM->m[1][3] * pV->y + pM->m[2][3] * pV->z + pM->m[3][3];
    out.x = (pM->m[0][0] * pV->x + pM->m[1][0] * pV->y + pM->m[2][0] * pV->z + pM->m[3][0]) / norm;
    out.y = (pM->m[0][1] * pV->x + pM->m[1][1] * pV->y + pM->m[2][1] * pV->z + pM->m[3][1]) / norm;
    out.z = (pM->m[0][2] * pV->x + pM->m[1][2] * pV->y + pM->m[2][2] * pV->z + pM->m[3][2]) / norm;
    *pOut = out;
    return pOut;
}

D3DXVECTOR3 *D3DXVec3TransformNormal(D3DXVECTOR3 *pOut, const D3DXVECTOR3 *pV, const D3DXMATRIX *pM)
{
    D3DXVECTOR3 out;
    out.x = pM->m[0][0] * pV->x + pM->m[1][0] * pV->y + pM->m[2][0] * pV->z;
    out.y = pM->m[0][1] * pV->x + pM->m[1][1] * pV->y + pM->m[2][1] * pV->z;
    out.z = pM->m[0][2] * pV->x + pM->m[1][2] * pV->y + pM->m[2][2] * pV->z;
    *pOut = out;
    return pOut;
}

D3DXMATRIX *D3DXMatrixMultiply(D3DXMATRIX *pOut, const D3DXMATRIX *pM1, const D3DXMATRIX *pM2)
{
    D3DXMATRIX out;
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            out.m[i][j] = pM1->m[i][0] * pM2->m[0][j] + pM1->m[i][1] * pM2->m[1][j] + pM1->m[i][2] * pM2->m[2][j] +
                          pM1->m[i][3] * pM2->m[3][j];
        }
    }
    *pOut = out;
    return pOut;
}

D3DXVECTOR3 *D3DXVec3Project(D3DXVECTOR3 *pOut, const D3DXVECTOR3 *pV, const D3DVIEWPORT9 *pViewport,
                             const D3DXMATRIX *pProjection, const D3DXMATRIX *pView, const D3DXMATRIX *pWorld)
{
    D3DXMATRIX m;
    D3DXMatrixIdentity(&m);
    if (pWorld != NULL)
    {
        D3DXMatrixMultiply(&m, &m, pWorld);
    }
    if (pView != NULL)
    {
        D3DXMatrixMultiply(&m, &m, pView);
    }
    if (pProjection != NULL)
    {
        D3DXMatrixMultiply(&m, &m, pProjection);
    }
    D3DXVec3TransformCoord(pOut, pV, &m);
    if (pViewport != NULL)
    {
        pOut->x = pViewport->X + (1.0f + pOut->x) * pViewport->Width / 2.0f;
        pOut->y = pViewport->Y + (1.0f - pOut->y) * pViewport->Height / 2.0f;
        pOut->z = pViewport->MinZ + pOut->z * (pViewport->MaxZ - pViewport->MinZ);
    }
    return pOut;
}

D3DXVECTOR3 *D3DXVec3ProjectArray(D3DXVECTOR3 *pOut, UINT OutStride, const D3DXVECTOR3 *pV, UINT VStride,
                                  const D3DVIEWPORT9 *pViewport, const D3DXMATRIX *pProjection,
                                  const D3DXMATRIX *pView, const D3DXMATRIX *pWorld, UINT n)
{
    for (UINT i = 0; i < n; i++)
    {
        D3DXVec3Project((D3DXVECTOR3 *)((char *)pOut + OutStride * i),
                        (const D3DXVECTOR3 *)((const char *)pV + VStride * i), pViewport, pProjection, pView, pWorld);
    }
    return pOut;
}

D3DXMATRIX *D3DXMatrixTranspose(D3DXMATRIX *pOut, const D3DXMATRIX *pM)
{
    D3DXMATRIX out;
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            out.m[i][j] = pM->m[j][i];
        }
    }
    *pOut = out;
    return pOut;
}

D3DXMATRIX *D3DXMatrixInverse(D3DXMATRIX *pOut, FLOAT *pDeterminant, const D3DXMATRIX *pM)
{
    // Cofactor expansion (as in Wine).
    const FLOAT(*m)[4] = pM->m;
    FLOAT inv[16];
    inv[0] = m[1][1] * m[2][2] * m[3][3] - m[1][1] * m[2][3] * m[3][2] - m[2][1] * m[1][2] * m[3][3] +
             m[2][1] * m[1][3] * m[3][2] + m[3][1] * m[1][2] * m[2][3] - m[3][1] * m[1][3] * m[2][2];
    inv[4] = -m[1][0] * m[2][2] * m[3][3] + m[1][0] * m[2][3] * m[3][2] + m[2][0] * m[1][2] * m[3][3] -
             m[2][0] * m[1][3] * m[3][2] - m[3][0] * m[1][2] * m[2][3] + m[3][0] * m[1][3] * m[2][2];
    inv[8] = m[1][0] * m[2][1] * m[3][3] - m[1][0] * m[2][3] * m[3][1] - m[2][0] * m[1][1] * m[3][3] +
             m[2][0] * m[1][3] * m[3][1] + m[3][0] * m[1][1] * m[2][3] - m[3][0] * m[1][3] * m[2][1];
    inv[12] = -m[1][0] * m[2][1] * m[3][2] + m[1][0] * m[2][2] * m[3][1] + m[2][0] * m[1][1] * m[3][2] -
              m[2][0] * m[1][2] * m[3][1] - m[3][0] * m[1][1] * m[2][2] + m[3][0] * m[1][2] * m[2][1];
    inv[1] = -m[0][1] * m[2][2] * m[3][3] + m[0][1] * m[2][3] * m[3][2] + m[2][1] * m[0][2] * m[3][3] -
             m[2][1] * m[0][3] * m[3][2] - m[3][1] * m[0][2] * m[2][3] + m[3][1] * m[0][3] * m[2][2];
    inv[5] = m[0][0] * m[2][2] * m[3][3] - m[0][0] * m[2][3] * m[3][2] - m[2][0] * m[0][2] * m[3][3] +
             m[2][0] * m[0][3] * m[3][2] + m[3][0] * m[0][2] * m[2][3] - m[3][0] * m[0][3] * m[2][2];
    inv[9] = -m[0][0] * m[2][1] * m[3][3] + m[0][0] * m[2][3] * m[3][1] + m[2][0] * m[0][1] * m[3][3] -
             m[2][0] * m[0][3] * m[3][1] - m[3][0] * m[0][1] * m[2][3] + m[3][0] * m[0][3] * m[2][1];
    inv[13] = m[0][0] * m[2][1] * m[3][2] - m[0][0] * m[2][2] * m[3][1] - m[2][0] * m[0][1] * m[3][2] +
              m[2][0] * m[0][2] * m[3][1] + m[3][0] * m[0][1] * m[2][2] - m[3][0] * m[0][2] * m[2][1];
    inv[2] = m[0][1] * m[1][2] * m[3][3] - m[0][1] * m[1][3] * m[3][2] - m[1][1] * m[0][2] * m[3][3] +
             m[1][1] * m[0][3] * m[3][2] + m[3][1] * m[0][2] * m[1][3] - m[3][1] * m[0][3] * m[1][2];
    inv[6] = -m[0][0] * m[1][2] * m[3][3] + m[0][0] * m[1][3] * m[3][2] + m[1][0] * m[0][2] * m[3][3] -
             m[1][0] * m[0][3] * m[3][2] - m[3][0] * m[0][2] * m[1][3] + m[3][0] * m[0][3] * m[1][2];
    inv[10] = m[0][0] * m[1][1] * m[3][3] - m[0][0] * m[1][3] * m[3][1] - m[1][0] * m[0][1] * m[3][3] +
              m[1][0] * m[0][3] * m[3][1] + m[3][0] * m[0][1] * m[1][3] - m[3][0] * m[0][3] * m[1][1];
    inv[14] = -m[0][0] * m[1][1] * m[3][2] + m[0][0] * m[1][2] * m[3][1] + m[1][0] * m[0][1] * m[3][2] -
              m[1][0] * m[0][2] * m[3][1] - m[3][0] * m[0][1] * m[1][2] + m[3][0] * m[0][2] * m[1][1];
    inv[3] = -m[0][1] * m[1][2] * m[2][3] + m[0][1] * m[1][3] * m[2][2] + m[1][1] * m[0][2] * m[2][3] -
             m[1][1] * m[0][3] * m[2][2] - m[2][1] * m[0][2] * m[1][3] + m[2][1] * m[0][3] * m[1][2];
    inv[7] = m[0][0] * m[1][2] * m[2][3] - m[0][0] * m[1][3] * m[2][2] - m[1][0] * m[0][2] * m[2][3] +
             m[1][0] * m[0][3] * m[2][2] + m[2][0] * m[0][2] * m[1][3] - m[2][0] * m[0][3] * m[1][2];
    inv[11] = -m[0][0] * m[1][1] * m[2][3] + m[0][0] * m[1][3] * m[2][1] + m[1][0] * m[0][1] * m[2][3] -
              m[1][0] * m[0][3] * m[2][1] - m[2][0] * m[0][1] * m[1][3] + m[2][0] * m[0][3] * m[1][1];
    inv[15] = m[0][0] * m[1][1] * m[2][2] - m[0][0] * m[1][2] * m[2][1] - m[1][0] * m[0][1] * m[2][2] +
              m[1][0] * m[0][2] * m[2][1] + m[2][0] * m[0][1] * m[1][2] - m[2][0] * m[0][2] * m[1][1];
    FLOAT det = m[0][0] * inv[0] + m[0][1] * inv[4] + m[0][2] * inv[8] + m[0][3] * inv[12];
    if (pDeterminant != NULL)
    {
        *pDeterminant = det;
    }
    if (det == 0.0f)
    {
        return NULL;
    }
    FLOAT scale = 1.0f / det;
    for (int i = 0; i < 16; i++)
    {
        pOut->m[i / 4][i % 4] = inv[i] * scale;
    }
    return pOut;
}

D3DXMATRIX *D3DXMatrixScaling(D3DXMATRIX *pOut, FLOAT sx, FLOAT sy, FLOAT sz)
{
    D3DXMatrixIdentity(pOut);
    pOut->m[0][0] = sx;
    pOut->m[1][1] = sy;
    pOut->m[2][2] = sz;
    return pOut;
}

D3DXMATRIX *D3DXMatrixTranslation(D3DXMATRIX *pOut, FLOAT x, FLOAT y, FLOAT z)
{
    D3DXMatrixIdentity(pOut);
    pOut->m[3][0] = x;
    pOut->m[3][1] = y;
    pOut->m[3][2] = z;
    return pOut;
}

D3DXMATRIX *D3DXMatrixRotationX(D3DXMATRIX *pOut, FLOAT Angle)
{
    FLOAT s = sinf(Angle);
    FLOAT c = cosf(Angle);
    D3DXMatrixIdentity(pOut);
    pOut->m[1][1] = c;
    pOut->m[1][2] = s;
    pOut->m[2][1] = -s;
    pOut->m[2][2] = c;
    return pOut;
}

D3DXMATRIX *D3DXMatrixRotationY(D3DXMATRIX *pOut, FLOAT Angle)
{
    FLOAT s = sinf(Angle);
    FLOAT c = cosf(Angle);
    D3DXMatrixIdentity(pOut);
    pOut->m[0][0] = c;
    pOut->m[0][2] = -s;
    pOut->m[2][0] = s;
    pOut->m[2][2] = c;
    return pOut;
}

D3DXMATRIX *D3DXMatrixRotationZ(D3DXMATRIX *pOut, FLOAT Angle)
{
    FLOAT s = sinf(Angle);
    FLOAT c = cosf(Angle);
    D3DXMatrixIdentity(pOut);
    pOut->m[0][0] = c;
    pOut->m[0][1] = s;
    pOut->m[1][0] = -s;
    pOut->m[1][1] = c;
    return pOut;
}

D3DXMATRIX *D3DXMatrixLookAtLH(D3DXMATRIX *pOut, const D3DXVECTOR3 *pEye, const D3DXVECTOR3 *pAt,
                               const D3DXVECTOR3 *pUp)
{
    D3DXVECTOR3 right;
    D3DXVECTOR3 up;
    D3DXVECTOR3 view;
    D3DXVECTOR3 diff = *pAt - *pEye;
    D3DXVec3Normalize(&view, &diff);
    D3DXVec3Cross(&right, pUp, &view);
    D3DXVec3Cross(&up, &view, &right);
    D3DXVec3Normalize(&right, &right);
    D3DXVec3Normalize(&up, &up);
    pOut->m[0][0] = right.x;
    pOut->m[1][0] = right.y;
    pOut->m[2][0] = right.z;
    pOut->m[3][0] = -D3DXVec3Dot(&right, pEye);
    pOut->m[0][1] = up.x;
    pOut->m[1][1] = up.y;
    pOut->m[2][1] = up.z;
    pOut->m[3][1] = -D3DXVec3Dot(&up, pEye);
    pOut->m[0][2] = view.x;
    pOut->m[1][2] = view.y;
    pOut->m[2][2] = view.z;
    pOut->m[3][2] = -D3DXVec3Dot(&view, pEye);
    pOut->m[0][3] = 0.0f;
    pOut->m[1][3] = 0.0f;
    pOut->m[2][3] = 0.0f;
    pOut->m[3][3] = 1.0f;
    return pOut;
}

D3DXMATRIX *D3DXMatrixPerspectiveFovLH(D3DXMATRIX *pOut, FLOAT fovy, FLOAT Aspect, FLOAT zn, FLOAT zf)
{
    D3DXMatrixIdentity(pOut);
    pOut->m[0][0] = 1.0f / (Aspect * tanf(fovy / 2.0f));
    pOut->m[1][1] = 1.0f / tanf(fovy / 2.0f);
    pOut->m[2][2] = zf / (zf - zn);
    pOut->m[2][3] = 1.0f;
    pOut->m[3][2] = (zf * zn) / (zn - zf);
    pOut->m[3][3] = 0.0f;
    return pOut;
}

D3DXMATRIX *D3DXMatrixOrthoOffCenterLH(D3DXMATRIX *pOut, FLOAT l, FLOAT r, FLOAT b, FLOAT t, FLOAT zn, FLOAT zf)
{
    D3DXMatrixIdentity(pOut);
    pOut->m[0][0] = 2.0f / (r - l);
    pOut->m[1][1] = 2.0f / (t - b);
    pOut->m[2][2] = 1.0f / (zf - zn);
    pOut->m[3][0] = -1.0f - 2.0f * l / (r - l);
    pOut->m[3][1] = 1.0f + 2.0f * t / (b - t);
    pOut->m[3][2] = zn / (zn - zf);
    return pOut;
}

} // extern "C"
