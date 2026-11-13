#pragma once
#include "../core/vector_math.h"
#include "scope_optic_packet.h"
#include "scope_optic_math.h"
#include "../telemetry/armed_contact_witness.h"
// Candidate/calibration VALUES only. No automatic qualification, native calls,
// camera-offset fallback, reticle/HUD update or packet ACTIVE/GEOMETRY flag write.
namespace gx_sniper_lens
{
struct V
{
    double x, y, z;
    V() : x(0), y(0), z(0) {}
    V(double a, double b, double c) : x(a), y(b), z(c) {}
};
inline V add(V a, V b)
{
    return V(a.x + b.x, a.y + b.y, a.z + b.z);
}
inline V mul(V a, double s)
{
    return V(a.x * s, a.y * s, a.z * s);
}
inline double dot(V a, V b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
inline V cross(V a, V b)
{
    return gx_math::crossProduct(a, b);
}
inline bool finite(V a)
{
    return gx_scope_optic::finite(a.x) && gx_scope_optic::finite(a.y) && gx_scope_optic::finite(a.z);
}
inline bool unit(V a, V& out)
{
    double d = dot(a, a);
    if (!finite(a) || !gx_scope_optic::finite(d) || d < 1e-12)
        return false;
    out = mul(a, 1 / sqrt(d));
    return true;
}
struct LocalRim
{
    V center, normal, upHint;
    double radius;
    int axisSign;
    LocalRim() : upHint(0, 0, 1), radius(0), axisSign(0) {}
};
struct WorldCandidate
{
    V center, normal;
    double radius, worldLens[16];
    bool directed;
    WorldCandidate() : radius(0), directed(false)
    {
        memset(worldLens, 0, sizeof(worldLens));
    }
};
inline V rotate(const float* m, V v)
{
    return V(m[0] * v.x + m[3] * v.y + m[6] * v.z, m[1] * v.x + m[4] * v.y + m[7] * v.z,
             m[2] * v.x + m[5] * v.y + m[8] * v.z);
}
inline bool transform(unsigned weaponGroup, const float* exactVertexToWorld, const LocalRim& rim,
                      WorldCandidate& out)
{
    out = WorldCandidate();
    if (weaponGroup != GxNativeSniperGroup || !exactVertexToWorld ||
        !gx_armed_witness::rigid(exactVertexToWorld) || !finite(rim.center) || !finite(rim.normal) ||
        !gx_scope_optic::finite(rim.radius) || rim.radius < .0025 || rim.radius > .075)
        return false;
    WorldCandidate n;
    n.center = add(rotate(exactVertexToWorld, rim.center),
                   V(exactVertexToWorld[9], exactVertexToWorld[10], exactVertexToWorld[11]));
    if (!unit(rotate(exactVertexToWorld, rim.normal), n.normal))
        return false;
    n.radius = rim.radius;
    if (rim.axisSign == 0)
    {
        out = n;
        return true;
    } // unsigned static normal is a candidate only
    if (rim.axisSign != 1 && rim.axisSign != -1)
        return false;
    V forward = mul(n.normal, (double)rim.axisSign), up, right;
    V hint = rotate(exactVertexToWorld, rim.upHint);
    if (!unit(add(hint, mul(forward, -dot(hint, forward))), up) || !unit(cross(forward, up), right))
        return false;
    up = cross(right, forward);
    V back = mul(forward, -1);
    V rows[4] = {right, up, back, n.center};
    for (unsigned r = 0; r < 4; ++r)
    {
        n.worldLens[r * 4] = rows[r].x;
        n.worldLens[r * 4 + 1] = rows[r].y;
        n.worldLens[r * 4 + 2] = rows[r].z;
        n.worldLens[r * 4 + 3] = r == 3 ? 1. : 0.;
    }
    if (!gx_scope_optic::rigid(n.worldLens))
        return false;
    n.directed = true;
    out = n;
    return true;
}
// Orient undirected measured rims with the ALREADY qualified native muzzle
// direction (+muzzle.a in actual113 firing path). Rear/ocular end minimizes
// projection along this direction. No change to aiming or firing, and no
// wearer acceptance is inferred. Profile/model correspondence stays caller-owned.
inline bool selectRear(unsigned weaponGroup, const float* exactVertexToWorld, const LocalRim* a,
                       unsigned count, V barrelWorld, unsigned& selected, WorldCandidate& out)
{
    selected = 0;
    out = WorldCandidate();
    V barrel;
    if (!a || count < 1 || count > 8 || !unit(barrelWorld, barrel))
        return false;
    bool have = false;
    double best = 0;
    WorldCandidate result;
    for (unsigned i = 0; i < count; ++i)
    {
        LocalRim local = a[i];
        local.axisSign = 0;
        WorldCandidate candidate;
        if (!transform(weaponGroup, exactVertexToWorld, local, candidate))
            continue;
        const double alignment = dot(candidate.normal, barrel);
        if (fabs(alignment) < .9)
            continue;
        local.axisSign = alignment > 0 ? 1 : -1;
        if (!transform(weaponGroup, exactVertexToWorld, local, candidate))
            continue;
        double projection = dot(candidate.center, barrel);
        if (!have || projection < best)
        {
            have = true;
            best = projection;
            selected = i;
            result = candidate;
        }
    }
    if (!have)
        return false;
    out = result;
    return true;
}
// Fill only lens fields in a caller-owned, INACTIVE packet. MAIN alone can
// qualify matching live geometry/pose/profile/cameraReference/xrReference.
inline bool stage(const WorldCandidate& candidate, GxScopeOptic& packet)
{
    if (packet.flags || packet.weaponGroup != GxNativeSniperGroup || !candidate.directed ||
        !gx_scope_optic::rigid(candidate.worldLens) || !gx_scope_optic::finite(candidate.radius) ||
        candidate.radius < .0025 || candidate.radius > .075)
        return false;
    memcpy(packet.worldLens, candidate.worldLens, sizeof(packet.worldLens));
    packet.radius = candidate.radius;
    return true;
}
}
