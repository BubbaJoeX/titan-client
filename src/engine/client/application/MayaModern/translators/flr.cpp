#include "flr.h"
#include "SwgTranslatorNames.h"

#include "CollisionEnums.h"
#include "FloorTri.h"
#include "Iff.h"
#include "Globals.h"
#include "Misc.h"
#include "Tag.h"
#include "Vector.h"
#include "MayaSceneBuilder.h"
#include "MayaUtility.h"
#include "SwgImportTrace.h"

#include <maya/MDagPath.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MItDag.h>
#include <maya/MMatrix.h>
#include <maya/MPlug.h>
#include <maya/MPoint.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnMesh.h>
#include <maya/MDagModifier.h>
#include <maya/MGlobal.h>
#include <maya/MPointArray.h>
#include <maya/MIntArray.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MSelectionList.h>
#include <maya/MStatus.h>
#include <maya/MStringArray.h>
#include <maya/MPxFileTranslator.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {
    const Tag TAG_FLOR = TAG(F,L,O,R);
    const Tag TAG_VERT = TAG(V,E,R,T);
    const Tag TAG_TRIS = TAG(T,R,I,S);
    const Tag TAG_BEDG = TAG(B,E,D,G);
    const Tag TAG_PNOD = TAG(P,N,O,D);
    const Tag TAG_PEDG = TAG(P,E,D,G);

    static void flrLog(const char* fmt, ...)
    {
        va_list args;
        va_start(args, fmt);
        SwgImportTrace::logV("FlrTranslator", fmt, args);
        va_end(args);
    }

    static void skipOneIffBlock(Iff& iff)
    {
        if (iff.getNumberOfBlocksLeft() <= 0)
            return;
        if (iff.isCurrentForm())
        {
            iff.enterForm();
            while (iff.getNumberOfBlocksLeft() > 0)
                skipOneIffBlock(iff);
            iff.exitForm();
        }
        else
        {
            iff.enterChunk();
            iff.exitChunk();
        }
    }

    // ------------------------------------------------------------------
    // FLR export: mirrors sharedCollision FloorMesh::compile (scrub, merge, link, crossable edges, part tags,
    // portal edges) without pulling the full collision library into the plug-in.

    bool findPobCellAncestor(const MDagPath& path, MDagPath& outCell)
    {
        MDagPath p = path;
        while (p.length() > 1)
        {
            p.pop(1);
            const MString nm = MFnDagNode(p.node()).name();
            const char* c = nm.asChar();
            if (nm.length() >= 2 && c[0] == 'r' && c[1] >= '0' && c[1] <= '9' && p.length() > 1)
            {
                outCell = p;
                return true;
            }
        }
        return false;
    }

    float distPointSegment(const Vector& p, const Vector& a, const Vector& b)
    {
        const Vector ab(b.x - a.x, b.y - a.y, b.z - a.z);
        const Vector ap(p.x - a.x, p.y - a.y, p.z - a.z);
        const float len2 = ab.x * ab.x + ab.y * ab.y + ab.z * ab.z;
        float t = len2 > 1e-12f ? (ap.x * ab.x + ap.y * ab.y + ap.z * ab.z) / len2 : 0.f;
        t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
        const float dx = a.x + ab.x * t - p.x, dy = a.y + ab.y * t - p.y, dz = a.z + ab.z * t - p.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    /// Distance from p to a planar portal polygon (to its plane when p projects inside the outline,
    /// else to the nearest outline edge).
    float distPointPortal(const Vector& p, const std::vector<Vector>& poly)
    {
        float best = 1e30f;
        for (size_t i = 0; i < poly.size(); ++i)
            best = std::min(best, distPointSegment(p, poly[i], poly[(i + 1) % poly.size()]));
        if (poly.size() < 3) return best;
        const Vector& o = poly[0];
        const Vector e1(poly[1].x - o.x, poly[1].y - o.y, poly[1].z - o.z);
        const Vector e2(poly[2].x - o.x, poly[2].y - o.y, poly[2].z - o.z);
        Vector n(e1.y * e2.z - e1.z * e2.y, e1.z * e2.x - e1.x * e2.z, e1.x * e2.y - e1.y * e2.x);
        const float nl = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
        if (nl < 1e-9f) return best;
        n.x /= nl; n.y /= nl; n.z /= nl;
        const float d = (p.x - o.x) * n.x + (p.y - o.y) * n.y + (p.z - o.z) * n.z;
        const Vector q(p.x - n.x * d, p.y - n.y * d, p.z - n.z * d);
        int sign = 0;
        for (size_t i = 0; i < poly.size(); ++i)
        {
            const Vector& a = poly[i];
            const Vector& b = poly[(i + 1) % poly.size()];
            const Vector ab(b.x - a.x, b.y - a.y, b.z - a.z), aq(q.x - a.x, q.y - a.y, q.z - a.z);
            const float s = (ab.y * aq.z - ab.z * aq.y) * n.x + (ab.z * aq.x - ab.x * aq.z) * n.y + (ab.x * aq.y - ab.y * aq.x) * n.z;
            const int sg = s > 1e-6f ? 1 : (s < -1e-6f ? -1 : 0);
            if (sg == 0) continue;
            if (sign == 0) sign = sg;
            else if (sg != sign) return best;
        }
        return std::min(best, std::fabs(d));
    }

    class FloorBuilder
    {
    public:
        int addVertex(const Vector& v)
        {
            // merge coincident verts (FloorMesh::merge)
            for (size_t i = 0; i < m_verts.size(); ++i)
            {
                const Vector& o = m_verts[i];
                if (std::fabs(o.x - v.x) < 1e-4f && std::fabs(o.y - v.y) < 1e-4f && std::fabs(o.z - v.z) < 1e-4f)
                    return static_cast<int>(i);
            }
            m_verts.push_back(v);
            return static_cast<int>(m_verts.size() - 1);
        }

        void addTriangle(int a, int b, int c)
        {
            if (a == b || b == c || a == c) return;
            const Vector& A = m_verts[static_cast<size_t>(a)];
            const Vector& B = m_verts[static_cast<size_t>(b)];
            const Vector& C = m_verts[static_cast<size_t>(c)];
            const Vector e1(B.x - A.x, B.y - A.y, B.z - A.z), e2(C.x - A.x, C.y - A.y, C.z - A.z);
            const float cx = e1.y * e2.z - e1.z * e2.y, cy = e1.z * e2.x - e1.x * e2.z, cz = e1.x * e2.y - e1.y * e2.x;
            if (cx * cx + cy * cy + cz * cz < 1e-10f) return;  // scrub degenerate
            Tri t;
            t.c[0] = a; t.c[1] = b; t.c[2] = c;
            m_tris.push_back(t);
        }

        /// Open (unshared, non-portal) edges become FET_Crossable instead of FET_Uncrossable: the exterior floor
        /// (r0), whose rim meets the terrain, so the player can step on and off it like stock buildings.
        void setOpenEdgesCrossable(bool crossable) { m_openCrossable = crossable; }

        /// Collect the cell's `collision|extent` meshes as cell-space triangles; open floor edges running along
        /// the foot of one of these walls are written as FET_WallBase (stock floors tag wall feet that way).
        void collectCellWalls(const MDagPath& cellPath)
        {
            const MMatrix cellInverse = cellPath.inclusiveMatrixInverse();
            MItDag it;
            it.reset(cellPath, MItDag::kDepthFirst, MFn::kMesh);
            for (; !it.isDone(); it.next())
            {
                MDagPath shapePath;
                it.getPath(shapePath);
                const MString full = shapePath.fullPathName();
                if (strstr(full.asChar(), "|collision|extent|") == nullptr || MFnDagNode(shapePath).isIntermediateObject())
                    continue;
                MPointArray pts;
                MFnMesh(shapePath).getPoints(pts, MSpace::kObject);
                const MMatrix m = shapePath.inclusiveMatrix() * cellInverse;
                MItMeshPolygon polyIt(shapePath);
                MIntArray v;
                for (; !polyIt.isDone(); polyIt.next())
                {
                    polyIt.getVertices(v);
                    for (unsigned t = 1; t + 1 < v.length(); ++t)
                    {
                        WallTri w;
                        const int ids[3] = {v[0], v[t], v[t + 1]};
                        for (int k = 0; k < 3; ++k)
                        {
                            const MPoint q = pts[ids[k]] * m;
                            w.p[k] = Vector(static_cast<float>(-q.x), static_cast<float>(q.y), static_cast<float>(q.z));
                        }
                        m_walls.push_back(w);
                    }
                }
            }
        }

        /// Collect `rN|portals|*` portal meshes (with buildingPortalIndex) as cell-space polygons.
        void collectCellPortals(const MDagPath& cellPath)
        {
            const MMatrix cellInverse = cellPath.inclusiveMatrixInverse();
            MFnDagNode cellFn(cellPath);
            for (unsigned i = 0; i < cellFn.childCount(); ++i)
            {
                MObject grp = cellFn.child(i);
                if (MFnDagNode(grp).name() != "portals") continue;
                MDagPath grpPath;
                MFnDagNode(grp).getPath(grpPath);
                MItDag it;
                it.reset(grpPath, MItDag::kDepthFirst, MFn::kMesh);
                for (; !it.isDone(); it.next())
                {
                    MDagPath shapePath;
                    it.getPath(shapePath);
                    MDagPath xf = shapePath;
                    xf.pop(1);
                    MPlug idxPlug = MFnDependencyNode(xf.node()).findPlug("buildingPortalIndex", true);
                    if (idxPlug.isNull()) continue;
                    PortalPoly pp;
                    idxPlug.getValue(pp.id);
                    MPointArray pts;
                    MFnMesh(shapePath).getPoints(pts, MSpace::kObject);
                    const MMatrix m = shapePath.inclusiveMatrix() * cellInverse;
                    // Outline from the boundary: a portal quad split into two tris still has 4 border verts.
                    MIntArray ids;
                    MItMeshPolygon polyIt(shapePath);
                    if (polyIt.count() == 1)
                        polyIt.getVertices(ids);
                    else
                        outlineFromBorderEdges(shapePath, ids);
                    for (unsigned v = 0; v < ids.length(); ++v)
                    {
                        const MPoint p = pts[ids[v]] * m;
                        pp.verts.push_back(Vector(static_cast<float>(-p.x), static_cast<float>(p.y), static_cast<float>(p.z)));
                    }
                    if (pp.verts.size() >= 3)
                        m_portals.push_back(pp);
                }
            }
        }

        bool compile()
        {
            if (m_tris.empty()) return false;

            // Orientation: engine floor tris have cross(B-A, C-A).y > 0 (Maya import keeps index order while
            // negating X, so authored up-facing planes arrive reversed). Flip the whole mesh if most area faces down.
            double upArea = 0.0;
            for (const Tri& t : m_tris)
                upArea += crossY(t);
            if (upArea < 0.0)
                for (Tri& t : m_tris) std::swap(t.c[1], t.c[2]);

            // sweep unused verts
            std::vector<int> used(m_verts.size(), -1);
            std::vector<Vector> packed;
            for (Tri& t : m_tris)
                for (int k = 0; k < 3; ++k)
                {
                    int& c = t.c[k];
                    if (used[static_cast<size_t>(c)] < 0)
                    {
                        used[static_cast<size_t>(c)] = static_cast<int>(packed.size());
                        packed.push_back(m_verts[static_cast<size_t>(c)]);
                    }
                    c = used[static_cast<size_t>(c)];
                }
            m_verts.swap(packed);

            // link: edge k runs corner k -> corner k+1; the neighbour holds the reversed directed edge
            std::map<std::pair<int, int>, int> directed;
            for (size_t i = 0; i < m_tris.size(); ++i)
                for (int k = 0; k < 3; ++k)
                    directed[std::make_pair(m_tris[i].c[k], m_tris[i].c[(k + 1) % 3])] = static_cast<int>(i);
            for (size_t i = 0; i < m_tris.size(); ++i)
                for (int k = 0; k < 3; ++k)
                {
                    auto it = directed.find(std::make_pair(m_tris[i].c[(k + 1) % 3], m_tris[i].c[k]));
                    if (it != directed.end() && it->second != static_cast<int>(i))
                    {
                        m_tris[i].neighbor[k] = it->second;
                        m_tris[i].edgeType[k] = FET_Crossable;
                    }
                }

            // part tags: connected components through crossable edges
            int tag = 0;
            for (size_t i = 0; i < m_tris.size(); ++i)
            {
                if (m_tris[i].part != -1) continue;
                std::vector<int> stack(1, static_cast<int>(i));
                while (!stack.empty())
                {
                    const int t = stack.back();
                    stack.pop_back();
                    Tri& tri = m_tris[static_cast<size_t>(t)];
                    if (tri.part != -1) continue;
                    tri.part = tag;
                    for (int k = 0; k < 3; ++k)
                        if (tri.neighbor[k] >= 0)
                            stack.push_back(tri.neighbor[k]);
                }
                ++tag;
            }
            m_partCount = tag;

            // portal edges: boundary edges lying on a portal outline/plane (FloorMesh::matchSegmentToPoly tolerances)
            for (Tri& t : m_tris)
                for (int k = 0; k < 3; ++k)
                {
                    if (t.neighbor[k] >= 0) continue;
                    const Vector& a = m_verts[static_cast<size_t>(t.c[k])];
                    const Vector& b = m_verts[static_cast<size_t>(t.c[(k + 1) % 3])];
                    for (const PortalPoly& pp : m_portals)
                        if (distPointPortal(a, pp.verts) < 0.05f && distPointPortal(b, pp.verts) < 0.05f)
                        {
                            t.portalId[k] = pp.id;
                            ++m_portalEdges;
                            break;
                        }
                }

            // open edges: wall feet -> WallBase; the rest crossable on exterior floors
            for (Tri& t : m_tris)
                for (int k = 0; k < 3; ++k)
                {
                    if (t.neighbor[k] >= 0 || t.portalId[k] >= 0) continue;
                    const Vector& a = m_verts[static_cast<size_t>(t.c[k])];
                    const Vector& b = m_verts[static_cast<size_t>(t.c[(k + 1) % 3])];
                    // wall foot: every sample along the edge touches some wall (window/door cuts split a wall
                    // into many triangles, so no single one needs to cover the whole edge)
                    bool wall = !m_walls.empty();
                    for (int sIdx = 0; sIdx <= 4 && wall; ++sIdx)
                    {
                        const float f = static_cast<float>(sIdx) / 4.f;
                        const Vector q(a.x + (b.x - a.x) * f, a.y + (b.y - a.y) * f, a.z + (b.z - a.z) * f);
                        bool touch = false;
                        for (const WallTri& w : m_walls)
                            if (isWallFoot(w, q)) { touch = true; break; }
                        wall = touch;
                    }
                    if (wall)
                        t.edgeType[k] = FET_WallBase;
                    else if (m_openCrossable)
                        t.edgeType[k] = FET_Crossable;
                }
            return true;
        }

        void write(Iff& iff) const
        {
            iff.insertForm(TAG_FLOR);
            iff.insertForm(TAG_0006);
            iff.insertChunk(TAG_VERT);
            iff.insertChunkData(static_cast<int32>(m_verts.size()));
            for (const Vector& v : m_verts)
                iff.insertChunkFloatVector(v);
            iff.exitChunk(TAG_VERT);

            iff.insertChunk(TAG_TRIS);
            iff.insertChunkData(static_cast<int32>(m_tris.size()));
            for (size_t i = 0; i < m_tris.size(); ++i)
            {
                const Tri& t = m_tris[i];
                for (int k = 0; k < 3; ++k) iff.insertChunkData(static_cast<int32>(t.c[k]));
                iff.insertChunkData(static_cast<int32>(i));
                for (int k = 0; k < 3; ++k) iff.insertChunkData(static_cast<int32>(t.neighbor[k]));
                // v0006 stores cross(C-A, B-A) (FloorMesh::addTriangle); FloorMesh::read flips it for <= 6
                const Vector& A = m_verts[static_cast<size_t>(t.c[0])];
                const Vector& B = m_verts[static_cast<size_t>(t.c[1])];
                const Vector& C = m_verts[static_cast<size_t>(t.c[2])];
                const Vector u(C.x - A.x, C.y - A.y, C.z - A.z), w(B.x - A.x, B.y - A.y, B.z - A.z);
                Vector n(u.y * w.z - u.z * w.y, u.z * w.x - u.x * w.z, u.x * w.y - u.y * w.x);
                const float len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
                if (len > 1e-9f) { n.x /= len; n.y /= len; n.z /= len; }
                iff.insertChunkFloatVector(n);
                for (int k = 0; k < 3; ++k) iff.insertChunkData(static_cast<uint8>(t.edgeType[k]));
                iff.insertChunkData(static_cast<uint8>(0));  // fallthrough
                iff.insertChunkData(static_cast<int32>(t.part));
                for (int k = 0; k < 3; ++k) iff.insertChunkData(static_cast<int32>(t.portalId[k]));
            }
            iff.exitChunk(TAG_TRIS);

            // Boundary edge list (FloorMesh::buildBoundaryEdgeList / write). Not optional in practice:
            // FloorMesh::read_0006 peeks the next block name after TRIS without an end-of-form check.
            std::vector<std::pair<int, int>> boundary[3];  // crossable, uncrossable, wall base
            for (size_t i = 0; i < m_tris.size(); ++i)
                for (int k = 0; k < 3; ++k)
                    if (m_tris[i].neighbor[k] < 0 && m_tris[i].edgeType[k] <= FET_WallBase)
                        boundary[m_tris[i].edgeType[k] == FET_Crossable ? 0 : (m_tris[i].edgeType[k] == FET_Uncrossable ? 1 : 2)]
                            .push_back(std::make_pair(static_cast<int>(i), k));
            iff.insertChunk(TAG_BEDG);
            iff.insertChunkData(static_cast<int32>(boundary[0].size() + boundary[1].size() + boundary[2].size()));
            for (int group = 0; group < 3; ++group)
                for (const auto& e : boundary[group])
                {
                    iff.insertChunkData(static_cast<int32>(e.first));
                    iff.insertChunkData(static_cast<int32>(e.second));
                    iff.insertChunkData(static_cast<uint8>(group == 0 ? 1 : 0));
                }
            iff.exitChunk(TAG_BEDG);

            iff.exitForm(TAG_0006);
            iff.exitForm(TAG_FLOR);
        }

        size_t vertexCount() const { return m_verts.size(); }
        size_t triCount() const { return m_tris.size(); }
        int portalEdgeCount() const { return m_portalEdges; }
        int partCount() const { return m_partCount; }

    private:
        struct Tri
        {
            int c[3] = {-1, -1, -1};
            int neighbor[3] = {-1, -1, -1};
            int edgeType[3] = {FET_Uncrossable, FET_Uncrossable, FET_Uncrossable};
            int portalId[3] = {-1, -1, -1};
            int part = -1;
        };
        struct PortalPoly
        {
            int id = -1;
            std::vector<Vector> verts;
        };
        struct WallTri
        {
            Vector p[3];
        };

        static float distPointTri(const Vector& q, const WallTri& w)
        {
            // closest point on a triangle (Ericson, Real-Time Collision Detection 5.1.5)
            const Vector& a = w.p[0];
            const Vector& b = w.p[1];
            const Vector& c = w.p[2];
            auto sub = [](const Vector& u, const Vector& v) { return Vector(u.x - v.x, u.y - v.y, u.z - v.z); };
            auto dot = [](const Vector& u, const Vector& v) { return u.x * v.x + u.y * v.y + u.z * v.z; };
            auto at = [](const Vector& o, const Vector& d, float s) { return Vector(o.x + d.x * s, o.y + d.y * s, o.z + d.z * s); };
            const Vector ab = sub(b, a), ac = sub(c, a), ap = sub(q, a);
            Vector r;
            const float d1 = dot(ab, ap), d2 = dot(ac, ap);
            const Vector bp = sub(q, b), cp = sub(q, c);
            const float d3 = dot(ab, bp), d4 = dot(ac, bp), d5 = dot(ab, cp), d6 = dot(ac, cp);
            const float vc = d1 * d4 - d3 * d2, vb = d5 * d2 - d1 * d6, va = d3 * d6 - d5 * d4;
            if (d1 <= 0.f && d2 <= 0.f) r = a;
            else if (d3 >= 0.f && d4 <= d3) r = b;
            else if (d6 >= 0.f && d5 <= d6) r = c;
            else if (vc <= 0.f && d1 >= 0.f && d3 <= 0.f) r = at(a, ab, d1 / (d1 - d3));
            else if (vb <= 0.f && d2 >= 0.f && d6 <= 0.f) r = at(a, ac, d2 / (d2 - d6));
            else if (va <= 0.f && (d4 - d3) >= 0.f && (d5 - d6) >= 0.f)
                r = at(b, sub(c, b), (d4 - d3) / ((d4 - d3) + (d5 - d6)));
            else
            {
                const float den = 1.f / (va + vb + vc);
                r = at(at(a, ab, vb * den), ac, vc * den);
            }
            const Vector d = sub(q, r);
            return std::sqrt(dot(d, d));
        }

        /// Floor point q sits at the foot of wall triangle w: the wall is steep, rises well above q (not a kerb)
        /// and q touches it.
        static bool isWallFoot(const WallTri& w, const Vector& q)
        {
            const Vector e1(w.p[1].x - w.p[0].x, w.p[1].y - w.p[0].y, w.p[1].z - w.p[0].z);
            const Vector e2(w.p[2].x - w.p[0].x, w.p[2].y - w.p[0].y, w.p[2].z - w.p[0].z);
            const Vector n(e1.y * e2.z - e1.z * e2.y, e1.z * e2.x - e1.x * e2.z, e1.x * e2.y - e1.y * e2.x);
            const float len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
            if (len < 1e-8f || std::fabs(n.y) / len > 0.5f) return false;
            const float top = std::max(w.p[0].y, std::max(w.p[1].y, w.p[2].y));
            if (top < q.y + 0.25f) return false;
            return distPointTri(q, w) < 0.12f;
        }

        /// Ordered boundary loop of a triangulated planar portal mesh.
        static void outlineFromBorderEdges(const MDagPath& shapePath, MIntArray& outIds)
        {
            std::map<int, int> next;
            MItMeshPolygon polyIt(shapePath);
            MIntArray v;
            std::map<std::pair<int, int>, int> count;
            std::vector<std::pair<int, int>> edges;
            for (; !polyIt.isDone(); polyIt.next())
            {
                polyIt.getVertices(v);
                for (unsigned k = 0; k < v.length(); ++k)
                {
                    const int a = v[k], b = v[(k + 1) % v.length()];
                    edges.push_back(std::make_pair(a, b));
                    ++count[std::make_pair(std::min(a, b), std::max(a, b))];
                }
            }
            for (const auto& e : edges)
                if (count[std::make_pair(std::min(e.first, e.second), std::max(e.first, e.second))] == 1)
                    next[e.first] = e.second;
            if (next.empty()) return;
            const int start = next.begin()->first;
            int cur = start;
            for (size_t guard = 0; guard <= next.size(); ++guard)
            {
                outIds.append(cur);
                auto it = next.find(cur);
                if (it == next.end() || it->second == start) break;
                cur = it->second;
            }
        }

        double crossY(const Tri& t) const
        {
            const Vector& A = m_verts[static_cast<size_t>(t.c[0])];
            const Vector& B = m_verts[static_cast<size_t>(t.c[1])];
            const Vector& C = m_verts[static_cast<size_t>(t.c[2])];
            return static_cast<double>((B.z - A.z) * (C.x - A.x) - (B.x - A.x) * (C.z - A.z));
        }

        std::vector<Vector> m_verts;
        std::vector<Tri> m_tris;
        std::vector<PortalPoly> m_portals;
        std::vector<WallTri> m_walls;
        bool m_openCrossable = false;
        int m_portalEdges = 0;
        int m_partCount = 0;
    };

    static void appendTriCorners(const FloorTri& tri, std::vector<int>& indices)
    {
        indices.push_back(tri.getCornerIndex(0));
        indices.push_back(tri.getCornerIndex(1));
        indices.push_back(tri.getCornerIndex(2));
    }

    static bool readFlorVertices(Iff& iff, Tag versionTag, std::vector<Vector>& vertices)
    {
        iff.enterChunk(TAG_VERT);
        if (versionTag == TAG_0006)
        {
            const int vertexCount = iff.read_int32();
            vertices.resize(static_cast<size_t>(vertexCount));
            for (int i = 0; i < vertexCount; ++i)
                vertices[static_cast<size_t>(i)] = iff.read_floatVector();
        }
        else
        {
            while (iff.getChunkLengthLeft() > 0)
                vertices.push_back(iff.read_floatVector());
        }
        iff.exitChunk(TAG_VERT);
        return !vertices.empty();
    }

    static bool readFlorTriangles(Iff& iff, Tag versionTag, std::vector<int>& indices)
    {
        iff.enterChunk(TAG_TRIS);
        if (versionTag == TAG_0000)
        {
            while (iff.getChunkLengthLeft() > 0)
            {
                FloorTri tri;
                tri.setIndex(iff.read_int32());
                tri.setCornerIndex(0, iff.read_int32());
                tri.setCornerIndex(1, iff.read_int32());
                tri.setCornerIndex(2, iff.read_int32());
                iff.read_int32();
                iff.read_int32();
                iff.read_int32();
                iff.read_floatVector();
                appendTriCorners(tri, indices);
            }
        }
        else if (versionTag == TAG_0001)
        {
            while (iff.getChunkLengthLeft() > 0)
            {
                FloorTri tri;
                tri.setIndex(iff.read_int32());
                tri.setCornerIndex(0, iff.read_int32());
                tri.setCornerIndex(1, iff.read_int32());
                tri.setCornerIndex(2, iff.read_int32());
                iff.read_int32();
                iff.read_int32();
                iff.read_int32();
                iff.read_floatVector();
                iff.read_bool8();
                iff.read_bool8();
                iff.read_bool8();
                iff.read_bool8();
                appendTriCorners(tri, indices);
            }
        }
        else if (versionTag == TAG_0002)
        {
            while (iff.getChunkLengthLeft() > 0)
            {
                FloorTri tri;
                tri.read_0000(iff);
                appendTriCorners(tri, indices);
            }
        }
        else if (versionTag == TAG_0003 || versionTag == TAG_0004)
        {
            while (iff.getChunkLengthLeft() > 0)
            {
                FloorTri tri;
                tri.read_0001(iff);
                appendTriCorners(tri, indices);
            }
        }
        else if (versionTag == TAG_0005)
        {
            const int triCount = iff.getChunkLengthTotal() / FloorTri::getOnDiskSize_0001();
            for (int i = 0; i < triCount; ++i)
            {
                FloorTri tri;
                tri.read_0001(iff);
                appendTriCorners(tri, indices);
            }
        }
        else if (versionTag == TAG_0006)
        {
            const int triCount = iff.read_int32();
            for (int i = 0; i < triCount; ++i)
            {
                FloorTri tri;
                tri.read_0002(iff);
                appendTriCorners(tri, indices);
            }
        }
        else
        {
            iff.exitChunk(TAG_TRIS);
            return false;
        }
        iff.exitChunk(TAG_TRIS);
        return indices.size() >= 3;
    }

    static void skipFlorTailBlocks(Iff& iff, Tag versionTag)
    {
        while (!iff.atEndOfForm())
        {
            if (versionTag == TAG_0004 && !iff.isCurrentForm())
            {
                const Tag chunkTag = iff.getCurrentName();
                if (chunkTag == TAG_PNOD || chunkTag == TAG_PEDG)
                {
                    iff.enterChunk(chunkTag);
                    iff.exitChunk(chunkTag);
                    continue;
                }
            }
            skipOneIffBlock(iff);
        }
    }

    /**
     * Read FLOR/0000..0006 geometry (VERT + TRIS). Skips pathfinding / box-tree tail blocks.
     * Matches sharedCollision/FloorMesh.cpp version matrix.
     */
    static bool readFlorGeometry(Iff& iff, Tag versionTag, std::vector<Vector>& vertices, std::vector<int>& indices)
    {
        iff.enterForm(versionTag);
        if (!readFlorVertices(iff, versionTag, vertices))
        {
            iff.exitForm(versionTag);
            return false;
        }
        if (!readFlorTriangles(iff, versionTag, indices))
        {
            iff.exitForm(versionTag);
            return false;
        }
        skipFlorTailBlocks(iff, versionTag);
        iff.exitForm(versionTag);
        return true;
    }

    static bool loadFlorFile(const char* flrPath, std::vector<Vector>& vertices, std::vector<int>& indices)
    {
        if (!flrPath || !flrPath[0] || !Iff::isValid(flrPath))
            return false;
        Iff iff;
        if (!iff.open(flrPath, false))
            return false;
        if (iff.getCurrentName() != TAG_FLOR)
            return false;

        iff.enterForm(TAG_FLOR);
        const Tag versionTag = iff.getCurrentName();
        const bool ok =
            (versionTag == TAG_0000 || versionTag == TAG_0001 || versionTag == TAG_0002 ||
             versionTag == TAG_0003 || versionTag == TAG_0004 || versionTag == TAG_0005 ||
             versionTag == TAG_0006)
                ? readFlorGeometry(iff, versionTag, vertices, indices)
                : false;
        iff.exitForm(TAG_FLOR);
        return ok;
    }

    static MStatus buildFlrPreviewMesh(
        const std::vector<Vector>& vertices,
        const std::vector<int>& indices,
        const char* meshName,
        MObject parentObj,
        MDagPath& outPath)
    {
        if (vertices.empty() || indices.size() < 3)
            return MS::kFailure;

        std::vector<float> positions;
        positions.reserve(vertices.size() * 3);
        for (size_t i = 0; i < vertices.size(); ++i)
        {
            positions.push_back(vertices[i].x);
            positions.push_back(vertices[i].y);
            positions.push_back(vertices[i].z);
        }
        std::vector<float> normals(positions.size(), 0.0f);

        MayaSceneBuilder::ShaderGroupData sg;
        sg.shaderTemplateName = "shader/placeholder";
        for (size_t t = 0; t + 2 < indices.size(); t += 3)
        {
            MayaSceneBuilder::TriangleData tri;
            tri.indices[0] = indices[t];
            tri.indices[1] = indices[t + 1];
            tri.indices[2] = indices[t + 2];
            sg.triangles.push_back(tri);
        }
        std::vector<MayaSceneBuilder::ShaderGroupData> groups(1, sg);

        MStatus status = MayaSceneBuilder::createMesh(positions, normals, groups, meshName ? meshName : "floor", outPath);
        if (!status)
            return status;
        status = MayaSceneBuilder::assignPobCollisionPreviewMaterial(outPath);
        if (!status)
            flrLog("assignPobCollisionPreviewMaterial failed (floor preview shading)");
        if (!parentObj.isNull())
        {
            MDagPath meshTransformPath = outPath;
            if (meshTransformPath.hasFn(MFn::kMesh))
                meshTransformPath.pop(1);
            if (meshName)
                MFnDagNode(meshTransformPath.node()).setName(MString(meshName));  // createMesh only names the shape
            MDagModifier mod;
            status = mod.reparentNode(meshTransformPath.node(), parentObj);
            if (status)
                status = mod.doIt();
        }
        return status;
    }
}

void* FlrTranslator::creator()
{
    return new FlrTranslator();
}

MStatus FlrTranslator::createMeshFromFlr(const char* flrPath, const char* meshName, MObject parentObj, MDagPath& outPath)
{
    flrLog("createMeshFromFlr: %s", flrPath ? flrPath : "(null)");
    std::vector<Vector> vertices;
    std::vector<int> indices;
    if (!loadFlorFile(flrPath, vertices, indices))
    {
        flrLog("Failed to parse FLOR geometry");
        return MS::kFailure;
    }

    flrLog("Creating mesh: %zu vertices, %zu triangles", vertices.size(), indices.size() / 3);
    MStatus status = buildFlrPreviewMesh(vertices, indices, meshName, parentObj, outPath);
    if (!status)
        flrLog("createMesh failed");
    else
        flrLog("createMeshFromFlr OK");
    return status;
}

MStatus FlrTranslator::reader(const MFileObject& file, const MString& options, MPxFileTranslator::FileAccessMode mode)
{
    std::string fileName = MayaUtility::fileObjectPathForIdentify(file);
    if (fileName.empty())
        fileName = file.expandedFullName().asChar();
    flrLog("reader: %s", fileName.c_str());
    if (!Iff::isValid(fileName.c_str()))
    {
        std::cerr << fileName << " could not be read as a valid IFF file!" << std::endl;
        return MS::kFailure;
    }

    std::vector<Vector> vertices;
    std::vector<int> indices;
    if (!loadFlorFile(fileName.c_str(), vertices, indices))
    {
        std::cerr << "FlrTranslator: failed to parse FLOR geometry in " << fileName << std::endl;
        return MS::kFailure;
    }

    if (vertices.empty() || indices.size() < 3)
    {
        std::cerr << "FlrTranslator: no geometry in " << fileName << std::endl;
        return MS::kSuccess;
    }

    std::string meshName = fileName;
    const size_t lastSlash = meshName.find_last_of("/\\");
    if (lastSlash != std::string::npos)
        meshName = meshName.substr(lastSlash + 1);
    const size_t dot = meshName.find_last_of('.');
    if (dot != std::string::npos)
        meshName = meshName.substr(0, dot);

    flrLog("Creating mesh: %zu vertices, %zu triangles", vertices.size(), indices.size() / 3);
    MDagPath meshPath;
    MStatus status = buildFlrPreviewMesh(vertices, indices, meshName.c_str(), MObject::kNullObj, meshPath);
    if (!status)
    {
        flrLog("createMesh failed");
        std::cerr << "FlrTranslator: failed to create mesh" << std::endl;
        return MS::kFailure;
    }
    flrLog("reader OK");
    return MS::kSuccess;
}

/**
 * Handles writing out (exporting) the floor
 *
 * @param file the file to write
 * @param options the save options
 * @param mode the access mode of the file
 * @return the status of the operation
 */
MStatus FlrTranslator::writer(const MFileObject& file, const MString& options, MPxFileTranslator::FileAccessMode mode)
{
    const std::string fileName = file.expandedFullName().asChar();
    MSelectionList sel;
    MGlobal::getActiveSelectionList(sel);

    // Every selected mesh (or mesh under a selected transform) is merged into one floor.
    std::vector<MDagPath> meshPaths;
    for (unsigned i = 0; i < sel.length(); ++i)
    {
        MDagPath p;
        if (sel.getDagPath(i, p) != MS::kSuccess)
            continue;
        MItDag it;
        it.reset(p, MItDag::kDepthFirst, MFn::kMesh);
        for (; !it.isDone(); it.next())
        {
            MDagPath shapePath;
            it.getPath(shapePath);
            if (MFnDagNode(shapePath).isIntermediateObject())
                continue;
            meshPaths.push_back(shapePath);
        }
    }
    if (meshPaths.empty())
    {
        std::cerr << "FlrTranslator: select a floor mesh (or its parent transform) to export" << std::endl;
        return MS::kFailure;
    }

    // Floors inside a POB cell are written in cell (building) space, like exportPob portals.
    MDagPath cellPath;
    const bool inCell = findPobCellAncestor(meshPaths[0], cellPath);
    const MMatrix cellInverse = inCell ? cellPath.inclusiveMatrixInverse() : MMatrix::identity;

    FloorBuilder floor;
    for (const MDagPath& shapePath : meshPaths)
    {
        const MMatrix toFloor = inCell ? shapePath.inclusiveMatrix() * cellInverse : MMatrix::identity;
        MFnMesh meshFn(shapePath);
        MPointArray pts;
        if (meshFn.getPoints(pts, MSpace::kObject) != MS::kSuccess)
            continue;
        std::vector<int> remap(pts.length());
        for (unsigned i = 0; i < pts.length(); ++i)
        {
            const MPoint p = pts[i] * toFloor;
            remap[i] = floor.addVertex(Vector(static_cast<float>(-p.x), static_cast<float>(p.y), static_cast<float>(p.z)));
        }
        MItMeshPolygon polyIt(shapePath);
        MIntArray polyVerts;
        for (; !polyIt.isDone(); polyIt.next())
        {
            polyIt.getVertices(polyVerts);
            for (unsigned t = 1; t + 1 < polyVerts.length(); ++t)
                floor.addTriangle(remap[polyVerts[0]], remap[polyVerts[t]], remap[polyVerts[t + 1]]);
        }
    }

    if (inCell)
    {
        floor.collectCellPortals(cellPath);
        floor.collectCellWalls(cellPath);
        // r0 (the exterior) meets the terrain, so its open rim is walkable on/off. A floor mesh (or its
        // transform) can override that with a bool `swgFloorOpenEdgesCrossable`.
        MString name = MFnDagNode(cellPath).name();
        MPlug cellName = MFnDependencyNode(cellPath.node()).findPlug("pobCellName", true);
        if (!cellName.isNull()) cellName.getValue(name);
        bool crossable = (name == "r0");
        for (const MDagPath& shapePath : meshPaths)
        {
            MDagPath xf = shapePath;
            xf.pop(1);
            const MObject nodes[2] = {shapePath.node(), xf.node()};
            for (const MObject& node : nodes)
            {
                MPlug plug = MFnDependencyNode(node).findPlug("swgFloorOpenEdgesCrossable", true);
                if (!plug.isNull()) plug.getValue(crossable);
            }
        }
        floor.setOpenEdgesCrossable(crossable);
    }

    if (!floor.compile())
    {
        std::cerr << "FlrTranslator: no usable (non-degenerate) triangles to export" << std::endl;
        return MS::kFailure;
    }

    Iff iff(8192, true);
    floor.write(iff);
    if (!iff.write(fileName.c_str()))
    {
        std::cerr << "FlrTranslator: failed to write " << fileName << std::endl;
        return MS::kFailure;
    }
    std::cerr << "FlrTranslator: wrote " << fileName << " (" << floor.vertexCount() << " verts, " << floor.triCount()
              << " tris, " << floor.portalEdgeCount() << " portal edge(s), " << floor.partCount() << " part(s))" << std::endl;
    return MS::kSuccess;
}

/**
 * @return the file type this translator handles
 */
MString FlrTranslator::defaultExtension () const
{
    return "flr";
}

MString FlrTranslator::filter () const
{
    return MString(swg_translator::kFilterFlr);
}

/**
 * Validates if the provided file is one that this plug-in supports
 *
 * @param fileName the name of the file
 * @param buffer a buffer for reading into the file
 * @param size the size of the buffer
 * @return whether or not this file type is supported by this translator
 */
MPxFileTranslator::MFileKind FlrTranslator::identifyFile(const MFileObject& fileName, const char* buffer, short size) const
{
    const std::string pathStr = MayaUtility::fileObjectPathForIdentify(fileName);
    const int nameLength = static_cast<int>(pathStr.size());
    if (nameLength > 4 && !strcasecmp(pathStr.c_str() + nameLength - 4, ".flr"))
        return kCouldBeMyFileType;
    return kNotMyFileType;
}
