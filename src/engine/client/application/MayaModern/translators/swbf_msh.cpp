/**
 * SWBF (2004/2005) toolchain chunked .msh import — NOT SWG IFF .msh.
 * Chunk layout matches the format handled by:
 * https://github.com/PrismaticFlower/SWBF-msh-Blender-IO
 *
 * Geometry: SEGM (POSL/UV0L/UV1L/NRML/CLRL/NDXT/STRP/NDXL/WGHT/MATI),
 * GEOM/CLTH (CTEX/CPOS/CUV0/CMSH), GEOM/ENVL (bone-index remap for WGHT).
 * Vertex weights are parsed and remapped like Blender; Maya skin binding is not applied here.
 */

#include "swbf_msh.h"

#include "MayaSceneBuilder.h"
#include "MayaUtility.h"
#include "SwgTranslatorNames.h"

#include <maya/MColor.h>
#include <maya/MColorArray.h>
#include <maya/MFileObject.h>
#include <maya/MFloatArray.h>
#include <maya/MFloatPointArray.h>
#include <maya/MFnMesh.h>
#include <maya/MFnTransform.h>
#include <maya/MGlobal.h>
#include <maya/MIntArray.h>
#include <maya/MQuaternion.h>
#include <maya/MStatus.h>
#include <maya/MString.h>
#include <maya/MVector.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#if defined(_WIN32) && !defined(strcasecmp)
#define strcasecmp _stricmp
#endif
#if defined(_WIN32) && !defined(strncasecmp)
#define strncasecmp _strnicmp
#endif

namespace
{
constexpr uint32_t kMaxFileBytes = 256u * 1024u * 1024u;

struct Vec2
{
	float u = 0.f;
	float v = 0.f;
};

struct Vec3
{
	float x = 0.f;
	float y = 0.f;
	float z = 0.f;
};

struct Vec4
{
	float r = 1.f;
	float g = 1.f;
	float b = 1.f;
	float a = 1.f;
};

struct VertexWeightBf
{
	float weight = 0.f;
	uint32_t boneIndex = 0;
};

struct MaterialBf
{
	std::string name;
	std::string texture0;
};

struct SegmentBf
{
	std::string materialName;
	std::vector<Vec3> positions;
	std::vector<Vec3> normals;
	std::vector<Vec2> texcoords;
	std::vector<Vec2> texcoords1;
	/// Triangle corner indices into `positions` (supports cloth CMSH u32 indices).
	std::vector<uint32_t> triangles;
	std::vector<Vec4> vertexColors;
	std::vector<std::vector<VertexWeightBf>> weights;
};

struct ModelBf
{
	std::string name;
	std::string parentName;
	uint32_t modelType = 0;
	bool hidden = false;
	Vec3 scale {1.f, 1.f, 1.f};
	Vec3 translation{};
	float rot[4] = {0.f, 0.f, 0.f, 1.f};
	std::vector<SegmentBf> segments;
};

/// Battlefront file vector -> Maya Y-up (matches Blender addon convert_vector_space: (-x, z, y)).
static Vec3 toMayaPos(Vec3 const& v)
{
	return Vec3{-v.x, v.z, v.y};
}

/// File quaternion stored as x,y,z,w floats; Blender addon converts for Blender — Maya MQuaternion is (x,y,z,w).
static MQuaternion toMayaRot(float const rx, float ry, float rz, float rw)
{
	const float bx = rx;
	const float by = ry;
	const float bz = rz;
	const float bw = rw;
	const float mx = bx;
	const float my = -bz;
	const float mz = -by;
	const float mw = -bw;
	return MQuaternion(mx, my, mz, mw);
}

static bool readU8Span(const uint8_t*& p, const uint8_t* end, uint8_t& out)
{
	if (p >= end)
		return false;
	out = *p++;
	return true;
}

static bool readU32LE(const uint8_t*& p, const uint8_t* end, uint32_t& out)
{
	if (static_cast<size_t>(end - p) < 4)
		return false;
	out = static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16)
	      | (static_cast<uint32_t>(p[3]) << 24);
	p += 4;
	return true;
}

static bool readU16LE(const uint8_t*& p, const uint8_t* end, uint16_t& out)
{
	if (static_cast<size_t>(end - p) < 2)
		return false;
	out = static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
	p += 2;
	return true;
}

static bool readF32(const uint8_t*& p, const uint8_t* end, float& out)
{
	union
	{
		float f;
		uint32_t u;
	} u;
	uint32_t raw = 0;
	if (!readU32LE(p, end, raw))
		return false;
	u.u = raw;
	out = u.f;
	return true;
}

static bool readTag(const uint8_t*& p, const uint8_t* end, char outTag[4])
{
	if (static_cast<size_t>(end - p) < 4)
		return false;
	std::memcpy(outTag, p, 4);
	p += 4;
	return true;
}

static bool tagIsAscii(char const t[4])
{
	for (int i = 0; i < 4; ++i)
	{
		const unsigned char c = static_cast<unsigned char>(t[i]);
		if (c < 32 || c > 126)
			return false;
	}
	return true;
}

static bool readCString(const uint8_t*& p, const uint8_t* end, std::string& out)
{
	out.clear();
	while (p < end)
	{
		const uint8_t b = *p++;
		if (b == 0)
			return true;
		out.push_back(static_cast<char>(b));
	}
	return false;
}

static size_t skipUntilTag(uint8_t const* base, size_t fileSize, char const tag4[4])
{
	const size_t lim = fileSize >= 8 ? fileSize - 8 : 0;
	for (size_t off = 0; off < lim; ++off)
	{
		if (std::memcmp(base + off, tag4, 4) == 0)
			return off;
	}
	return static_cast<size_t>(-1);
}

static bool loadEntireFile(char const* path, std::vector<uint8_t>& out)
{
	std::ifstream f(path, std::ios::binary | std::ios::ate);
	if (!f)
		return false;
	const std::streamoff sz = f.tellg();
	if (sz <= 0 || static_cast<uint64_t>(sz) > static_cast<uint64_t>(kMaxFileBytes))
		return false;
	out.resize(static_cast<size_t>(sz));
	f.seekg(0);
	if (!f.read(reinterpret_cast<char*>(out.data()), sz))
		return false;
	return true;
}

static bool parseMatd(const uint8_t* p, const uint8_t* end, MaterialBf& mat)
{
	while (static_cast<size_t>(end - p) >= 8)
	{
		char chTag[4];
		uint32_t childSize = 0;
		if (!readTag(p, end, chTag))
			return false;
		if (!readU32LE(p, end, childSize))
			return false;
		const uint8_t* childEnd = p + childSize;
		if (childEnd > end)
			return false;

		if (std::memcmp(chTag, "NAME", 4) == 0)
		{
			readCString(p, childEnd, mat.name);
		}
		else if (std::memcmp(chTag, "TX0D", 4) == 0 && childEnd > p)
		{
			readCString(p, childEnd, mat.texture0);
		}
		p = childEnd;
	}
	return true;
}

static bool parseMatl(const uint8_t* p, const uint8_t* end, std::vector<MaterialBf>& materials)
{
	uint32_t num = 0;
	if (!readU32LE(p, end, num) || num > 65536)
		return false;
	materials.reserve(num);
	for (uint32_t i = 0; i < num; ++i)
	{
		if (static_cast<size_t>(end - p) < 8)
			return false;
		char matdTag[4];
		uint32_t matdSize = 0;
		if (!readTag(p, end, matdTag))
			return false;
		if (std::memcmp(matdTag, "MATD", 4) != 0)
			return false;
		if (!readU32LE(p, end, matdSize))
			return false;
		const uint8_t* matdEnd = p + matdSize;
		if (matdEnd > end)
			return false;
		MaterialBf m;
		parseMatd(p, matdEnd, m);
		materials.push_back(std::move(m));
		p = matdEnd;
	}
	return true;
}

static bool parseTran(const uint8_t* p, const uint8_t* end, Vec3& scaleOut, Vec3& trans, float rotOut[4])
{
	if (static_cast<size_t>(end - p) < 12 + 16 + 12)
		return false;
	readF32(p, end, scaleOut.x);
	readF32(p, end, scaleOut.y);
	readF32(p, end, scaleOut.z);
	float qx = 0.f, qy = 0.f, qz = 0.f, qw = 1.f;
	readF32(p, end, qx);
	readF32(p, end, qy);
	readF32(p, end, qz);
	readF32(p, end, qw);
	rotOut[0] = qx;
	rotOut[1] = qy;
	rotOut[2] = qz;
	rotOut[3] = qw;
	readF32(p, end, trans.x);
	readF32(p, end, trans.y);
	readF32(p, end, trans.z);
	return true;
}

/// SWBF STRP: expand one index run [start, endIdx) as a single triangle strip.
static void appendTrianglesFromTristrip(std::vector<uint16_t> const& indices, size_t start, size_t endIdx, std::vector<uint32_t>& trisOut)
{
	if (start + 2 >= endIdx)
		return;
	const uint16_t i0 = indices[start] & 0x7fffu;
	const uint16_t i1 = indices[start + 1] & 0x7fffu;
	for (size_t k = start + 2; k < endIdx; ++k)
	{
		const uint16_t i2 = indices[k] & 0x7fffu;
		const bool odd = ((k - (start + 2)) % 2) == 1;
		if (!odd)
		{
			trisOut.push_back(i0);
			trisOut.push_back(i1);
			trisOut.push_back(i2);
		}
		else
		{
			trisOut.push_back(i0);
			trisOut.push_back(i2);
			trisOut.push_back(i1);
		}
	}
}

/// Geometry-less grouping transforms named `null`, `null9`, etc. (common SWBF convention).
static bool isBattlefrontNullGroupingModel(std::string const& name, bool noGeometry)
{
	if (!noGeometry || name.size() < 4)
		return false;
	if (strcasecmp(name.c_str(), "null") == 0)
		return true;
	if (strncasecmp(name.c_str(), "null", 4) != 0)
		return false;
	for (size_t i = 4; i < name.size(); ++i)
	{
		if (!std::isdigit(static_cast<unsigned char>(name[i])))
			return false;
	}
	return true;
}

static float sanitizeScaleComponent(float s)
{
	if (!std::isfinite(s) || std::fabs(s) < 1e-20f)
		return 1.f;
	return s;
}

static std::string fileBasenameLower(std::string const& path)
{
	if (path.empty())
		return {};
	size_t slash = path.find_last_of("\\/");
	std::string b = (slash == std::string::npos) ? path : path.substr(slash + 1);
	for (char& c : b)
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	return b;
}

/// Match CTEX texture path to MATL entry (basename compare).
static void resolveTexPathToMaterialName(std::string const& texPath, std::vector<MaterialBf> const& materials, std::string& materialNameOut)
{
	if (texPath.empty())
		return;
	const std::string want = fileBasenameLower(texPath);
	for (MaterialBf const& m : materials)
	{
		if (!m.texture0.empty() && fileBasenameLower(m.texture0) == want)
		{
			materialNameOut = m.name;
			return;
		}
		if (!m.name.empty() && fileBasenameLower(m.name) == want)
		{
			materialNameOut = m.name;
			return;
		}
	}
	materialNameOut = texPath;
}

/// GEOM/ENVL bone-index remap applied to SEGM WGHT (matches Blender importer).
static void applyEnvelopeToSegmentWeights(SegmentBf& seg, std::vector<uint32_t> const& envelope)
{
	if (envelope.empty() || seg.weights.empty())
		return;
	for (auto& perVtx : seg.weights)
	{
		for (auto& vw : perVtx)
		{
			if (vw.boneIndex < envelope.size())
				vw.boneIndex = envelope[vw.boneIndex];
		}
	}
}

/// Cloth geometry (CLTH replaces SEGM); reads CPOS/CUV0/CMSH/CTEX per ze_filetypes / SWBF tooling docs.
static bool parseClth(const uint8_t* p, const uint8_t* end, SegmentBf& seg, std::vector<MaterialBf> const& materials)
{
	std::string texHint;
	while (static_cast<size_t>(end - p) >= 8)
	{
		char chTag[4];
		uint32_t childSize = 0;
		if (!readTag(p, end, chTag))
			break;
		if (!readU32LE(p, end, childSize))
			return false;
		const uint8_t* childEnd = p + childSize;
		if (childEnd > end)
			return false;

		if (std::memcmp(chTag, "CTEX", 4) == 0 && childEnd > p)
		{
			readCString(p, childEnd, texHint);
			resolveTexPathToMaterialName(texHint, materials, seg.materialName);
		}
		else if (std::memcmp(chTag, "CPOS", 4) == 0)
		{
			uint32_t numPos = 0;
			readU32LE(p, childEnd, numPos);
			if (numPos > 2000000)
				return false;
			seg.positions.resize(numPos);
			for (uint32_t i = 0; i < numPos; ++i)
			{
				readF32(p, childEnd, seg.positions[i].x);
				readF32(p, childEnd, seg.positions[i].y);
				readF32(p, childEnd, seg.positions[i].z);
			}
		}
		else if (std::memcmp(chTag, "CUV0", 4) == 0)
		{
			uint32_t numUv = 0;
			readU32LE(p, childEnd, numUv);
			if (numUv > 2000000)
				return false;
			seg.texcoords.resize(numUv);
			for (uint32_t i = 0; i < numUv; ++i)
			{
				readF32(p, childEnd, seg.texcoords[i].u);
				readF32(p, childEnd, seg.texcoords[i].v);
			}
		}
		else if (std::memcmp(chTag, "CMSH", 4) == 0)
		{
			uint32_t numTri = 0;
			if (!readU32LE(p, childEnd, numTri) || numTri > 10000000)
				return false;
			const size_t bytesLeft = static_cast<size_t>(childEnd - p);
			size_t stride = 12;
			if (numTri > 0)
			{
				if (bytesLeft >= numTri * 16)
					stride = 16;
				else if (bytesLeft < numTri * 12)
				{
					p = childEnd;
					continue;
				}
			}
			for (uint32_t t = 0; t < numTri; ++t)
			{
				uint32_t i0 = 0, i1 = 0, i2 = 0;
				if (!readU32LE(p, childEnd, i0) || !readU32LE(p, childEnd, i1) || !readU32LE(p, childEnd, i2))
					break;
				if (stride == 16 && p + 4 <= childEnd)
					p += 4;
				seg.triangles.push_back(i0);
				seg.triangles.push_back(i1);
				seg.triangles.push_back(i2);
			}
		}

		p = childEnd;
	}

	if (!texHint.empty() && seg.materialName.empty())
		resolveTexPathToMaterialName(texHint, materials, seg.materialName);

	return true;
}

static bool parseSegm(const uint8_t* p, const uint8_t* end, SegmentBf& seg, std::vector<MaterialBf> const& materials)
{
	uint32_t numPos = 0;
	while (static_cast<size_t>(end - p) >= 8)
	{
		char chTag[4];
		uint32_t childSize = 0;
		if (!readTag(p, end, chTag))
			break;
		if (!readU32LE(p, end, childSize))
			return false;
		const uint8_t* childEnd = p + childSize;
		if (childEnd > end)
			return false;

		if (std::memcmp(chTag, "MATI", 4) == 0)
		{
			uint32_t idx = 0;
			readU32LE(p, childEnd, idx);
			if (idx < materials.size())
				seg.materialName = materials[idx].name;
		}
		else if (std::memcmp(chTag, "POSL", 4) == 0)
		{
			readU32LE(p, childEnd, numPos);
			seg.positions.resize(numPos);
			for (uint32_t i = 0; i < numPos; ++i)
			{
				Vec3 v{};
				readF32(p, childEnd, v.x);
				readF32(p, childEnd, v.y);
				readF32(p, childEnd, v.z);
				seg.positions[i] = v;
			}
		}
		else if (std::memcmp(chTag, "NRML", 4) == 0)
		{
			uint32_t numN = 0;
			readU32LE(p, childEnd, numN);
			seg.normals.resize(numN);
			for (uint32_t i = 0; i < numN; ++i)
			{
				Vec3 v{};
				readF32(p, childEnd, v.x);
				readF32(p, childEnd, v.y);
				readF32(p, childEnd, v.z);
				seg.normals[i] = v;
			}
		}
		else if (std::memcmp(chTag, "UV0L", 4) == 0)
		{
			uint32_t numUv = 0;
			readU32LE(p, childEnd, numUv);
			seg.texcoords.resize(numUv);
			for (uint32_t i = 0; i < numUv; ++i)
			{
				Vec2 uv{};
				readF32(p, childEnd, uv.u);
				readF32(p, childEnd, uv.v);
				seg.texcoords[i] = uv;
			}
		}
		else if (std::memcmp(chTag, "UV1L", 4) == 0)
		{
			uint32_t numUv = 0;
			readU32LE(p, childEnd, numUv);
			seg.texcoords1.resize(numUv);
			for (uint32_t i = 0; i < numUv; ++i)
			{
				Vec2 uv{};
				readF32(p, childEnd, uv.u);
				readF32(p, childEnd, uv.v);
				seg.texcoords1[i] = uv;
			}
		}
		else if (std::memcmp(chTag, "CLRL", 4) == 0)
		{
			uint32_t nc = 0;
			readU32LE(p, childEnd, nc);
			if (nc > 2000000)
				return false;
			seg.vertexColors.resize(nc);
			for (uint32_t i = 0; i < nc; ++i)
			{
				uint32_t packed = 0;
				if (!readU32LE(p, childEnd, packed))
					break;
				Vec4& c = seg.vertexColors[i];
				c.r = static_cast<float>((packed >> 16) & 0xFF) / 255.f;
				c.g = static_cast<float>((packed >> 8) & 0xFF) / 255.f;
				c.b = static_cast<float>((packed >> 0) & 0xFF) / 255.f;
				c.a = static_cast<float>((packed >> 24) & 0xFF) / 255.f;
			}
		}
		else if (std::memcmp(chTag, "WGHT", 4) == 0)
		{
			uint32_t nw = 0;
			readU32LE(p, childEnd, nw);
			if (nw > 2000000)
				return false;
			seg.weights.resize(nw);
			for (uint32_t vi = 0; vi < nw; ++vi)
			{
				for (int k = 0; k < 4; ++k)
				{
					uint32_t bi = 0;
					float w = 0.f;
					if (!readU32LE(p, childEnd, bi) || !readF32(p, childEnd, w))
						break;
					if (w > 0.000001f)
						seg.weights[vi].push_back(VertexWeightBf{w, bi});
				}
			}
		}
		else if (std::memcmp(chTag, "NDXT", 4) == 0)
		{
			uint32_t numTris = 0;
			if (!readU32LE(p, childEnd, numTris))
				return false;
			const size_t bytesLeft = static_cast<size_t>(childEnd - p);
			// Some SWBF meshes store u32 triangle indices when vertex count exceeds 16-bit.
			if (numTris > 0 && bytesLeft >= static_cast<size_t>(numTris) * 12u)
			{
				for (uint32_t t = 0; t < numTris; ++t)
				{
					uint32_t i0 = 0, i1 = 0, i2 = 0;
					if (!readU32LE(p, childEnd, i0) || !readU32LE(p, childEnd, i1) || !readU32LE(p, childEnd, i2))
						break;
					seg.triangles.push_back(i0);
					seg.triangles.push_back(i1);
					seg.triangles.push_back(i2);
				}
			}
			else if (numTris > 0 && bytesLeft >= static_cast<size_t>(numTris) * 6u)
			{
				for (uint32_t t = 0; t < numTris; ++t)
				{
					uint16_t i0 = 0, i1 = 0, i2 = 0;
					if (!readU16LE(p, childEnd, i0) || !readU16LE(p, childEnd, i1) || !readU16LE(p, childEnd, i2))
						break;
					seg.triangles.push_back(static_cast<uint32_t>(i0));
					seg.triangles.push_back(static_cast<uint32_t>(i1));
					seg.triangles.push_back(static_cast<uint32_t>(i2));
				}
			}
		}
		else if (std::memcmp(chTag, "STRP", 4) == 0)
		{
			uint32_t numInd = 0;
			readU32LE(p, childEnd, numInd);
			if (numInd < 3)
			{
				p = childEnd;
				continue;
			}
			std::vector<uint16_t> indices(numInd);
			for (uint32_t i = 0; i < numInd; ++i)
				readU16LE(p, childEnd, indices[i]);

			// Strips start where two consecutive indices have the high bit set (0x8000).
			std::vector<size_t> stripStarts;
			for (uint32_t i = 0; i + 1 < numInd; ++i)
			{
				if ((indices[i] & 0x8000u) != 0 && (indices[i + 1] & 0x8000u) != 0)
					stripStarts.push_back(i);
			}
			stripStarts.push_back(numInd);

			// If no restart markers appear, some meshes store a single strip without 0x8000 pairs;
			// the previous logic left stripStarts == { numInd } only and emitted zero triangles.
			if (stripStarts.size() == 1u)
			{
				appendTrianglesFromTristrip(indices, 0, stripStarts[0], seg.triangles);
			}
			else
			{
				// Leading indices before the first 0x8000 pair are still the first tristrip (toolchain omits markers at offset 0).
				if (stripStarts[0] > 0)
					stripStarts.insert(stripStarts.begin(), static_cast<size_t>(0));
				for (size_t bi = 0; bi + 1 < stripStarts.size(); ++bi)
					appendTrianglesFromTristrip(indices, stripStarts[bi], stripStarts[bi + 1], seg.triangles);
			}
		}
		else if (std::memcmp(chTag, "NDXL", 4) == 0)
		{
			uint32_t numPoly = 0;
			readU32LE(p, childEnd, numPoly);
			for (uint32_t pi = 0; pi < numPoly; ++pi)
			{
				uint16_t ninds = 0;
				if (!readU16LE(p, childEnd, ninds))
					break;
				std::vector<uint16_t> inds(ninds);
				for (uint16_t j = 0; j < ninds; ++j)
					readU16LE(p, childEnd, inds[j]);
				if (ninds >= 3)
				{
					for (uint16_t j = 1; j + 1 < ninds; ++j)
					{
						seg.triangles.push_back(static_cast<uint32_t>(inds[0]));
						seg.triangles.push_back(static_cast<uint32_t>(inds[j]));
						seg.triangles.push_back(static_cast<uint32_t>(inds[j + 1]));
					}
				}
			}
		}

		p = childEnd;
	}
	return true;
}

static bool parseGeom(const uint8_t* p, const uint8_t* end, ModelBf& model, std::vector<MaterialBf> const& materials)
{
	std::vector<uint32_t> envelope;
	while (static_cast<size_t>(end - p) >= 8)
	{
		char chTag[4];
		uint32_t childSize = 0;
		if (!readTag(p, end, chTag))
			break;
		if (!readU32LE(p, end, childSize))
			return false;
		const uint8_t* childEnd = p + childSize;
		if (childEnd > end)
			return false;

		if (std::memcmp(chTag, "SEGM", 4) == 0)
		{
			SegmentBf seg;
			parseSegm(p, childEnd, seg, materials);
			if (!seg.positions.empty() && !seg.triangles.empty())
				model.segments.push_back(std::move(seg));
		}
		else if (std::memcmp(chTag, "ENVL", 4) == 0)
		{
			uint32_t nidx = 0;
			if (readU32LE(p, childEnd, nidx) && nidx < 2000000)
			{
				for (uint32_t ii = 0; ii < nidx; ++ii)
				{
					uint32_t ev = 0;
					if (!readU32LE(p, childEnd, ev))
						break;
					envelope.push_back(ev);
				}
			}
		}
		else if (std::memcmp(chTag, "CLTH", 4) == 0)
		{
			SegmentBf seg;
			parseClth(p, childEnd, seg, materials);
			if (!seg.positions.empty() && !seg.triangles.empty())
				model.segments.push_back(std::move(seg));
		}

		p = childEnd;
	}

	for (SegmentBf& seg : model.segments)
		applyEnvelopeToSegmentWeights(seg, envelope);

	return true;
}

static bool parseModl(const uint8_t* p, const uint8_t* end, ModelBf& model, std::vector<MaterialBf> const& materials)
{
	while (static_cast<size_t>(end - p) >= 8)
	{
		char chTag[4];
		uint32_t childSize = 0;
		if (!readTag(p, end, chTag))
			break;
		if (!readU32LE(p, end, childSize))
			return false;
		const uint8_t* childEnd = p + childSize;
		if (childEnd > end)
			return false;

		if (std::memcmp(chTag, "NAME", 4) == 0)
			readCString(p, childEnd, model.name);
		else if (std::memcmp(chTag, "PRNT", 4) == 0)
			readCString(p, childEnd, model.parentName);
		else if (std::memcmp(chTag, "MTYP", 4) == 0)
			readU32LE(p, childEnd, model.modelType);
		else if (std::memcmp(chTag, "FLGS", 4) == 0)
		{
			uint32_t fl = 0;
			readU32LE(p, childEnd, fl);
			model.hidden = (fl != 0);
		}
		else if (std::memcmp(chTag, "TRAN", 4) == 0)
			parseTran(p, childEnd, model.scale, model.translation, model.rot);
		else if (std::memcmp(chTag, "GEOM", 4) == 0)
			parseGeom(p, childEnd, model, materials);

		p = childEnd;
	}
	return true;
}

static bool parseMsh2(const uint8_t* p, const uint8_t* end, std::vector<MaterialBf>& materials, std::vector<ModelBf>& models)
{
	materials.clear();
	models.clear();
	while (static_cast<size_t>(end - p) >= 8)
	{
		char chTag[4];
		uint32_t childSize = 0;
		if (!readTag(p, end, chTag))
			break;
		if (!readU32LE(p, end, childSize))
			return false;
		const uint8_t* childEnd = p + childSize;
		if (childEnd > end)
			return false;

		if (std::memcmp(chTag, "MATL", 4) == 0)
			parseMatl(p, childEnd, materials);
		else if (std::memcmp(chTag, "MODL", 4) == 0)
		{
			ModelBf m;
			parseModl(p, childEnd, m, materials);
			models.push_back(std::move(m));
		}
		p = childEnd;
	}
	return true;
}

static bool parseSwbfMshBuffer(std::vector<uint8_t> const& buf, std::vector<MaterialBf>& materials, std::vector<ModelBf>& models)
{
	models.clear();
	materials.clear();
	const uint8_t* base = buf.data();
	const size_t sz = buf.size();
	const size_t hedrOff = skipUntilTag(base, sz, "HEDR");
	if (hedrOff == static_cast<size_t>(-1))
		return false;
	const uint8_t* p = base + hedrOff;
	const uint8_t* end = base + sz;
	char tag[4];
	uint32_t hedrPayload = 0;
	if (!readTag(p, end, tag) || std::memcmp(tag, "HEDR", 4) != 0)
		return false;
	if (!readU32LE(p, end, hedrPayload))
		return false;
	const uint8_t* hedrEnd = p + hedrPayload;
	if (hedrEnd > end)
		return false;

	while (static_cast<size_t>(hedrEnd - p) >= 8)
	{
		char chTag[4];
		uint32_t chSize = 0;
		if (!readTag(p, hedrEnd, chTag))
			break;
		if (!readU32LE(p, hedrEnd, chSize))
			return false;
		const uint8_t* chEnd = p + chSize;
		if (chEnd > hedrEnd)
			return false;

		if (std::memcmp(chTag, "MSH2", 4) == 0)
			parseMsh2(p, chEnd, materials, models);

		p = chEnd;
	}
	return true;
}

static void sortModelsParentFirst(std::vector<ModelBf>& models)
{
	std::unordered_set<std::string> names;
	for (ModelBf const& m : models)
		names.insert(m.name);

	std::vector<ModelBf> remaining = std::move(models);
	models.clear();
	std::unordered_set<std::string> placed;

	while (!remaining.empty())
	{
		const size_t beforeCount = models.size();
		for (auto it = remaining.begin(); it != remaining.end();)
		{
			const std::string& par = it->parentName;
			const bool root = par.empty() || names.find(par) == names.end();
			const bool parentReady = root || placed.find(par) != placed.end();
			if (parentReady)
			{
				models.push_back(std::move(*it));
				placed.insert(models.back().name);
				it = remaining.erase(it);
			}
			else
				++it;
		}
		if (models.size() == beforeCount)
		{
			for (auto& m : remaining)
			{
				models.push_back(std::move(m));
				placed.insert(models.back().name);
			}
			break;
		}
	}
}

static MStatus buildMeshesForModels(std::vector<ModelBf>& models, MString const& fileRawName)
{
	sortModelsParentFirst(models);

	std::unordered_map<std::string, MObject> nameToTransform;

	MFnTransform sceneRoot;
	MObject rootObj = sceneRoot.create();
	sceneRoot.setName(fileRawName.length() > 0 ? fileRawName : MString("swbf_msh_import"));

	for (ModelBf& mod : models)
	{
		MObject parentObj = rootObj;
		if (!mod.parentName.empty())
		{
			auto it = nameToTransform.find(mod.parentName);
			if (it != nameToTransform.end())
				parentObj = it->second;
		}

		// SWBF commonly uses geometry-less MODLs named "null", "null9", ... as hierarchy spacers (Blender: Empty).
		const bool noGeometry = mod.segments.empty();
		const bool nullSpacer = isBattlefrontNullGroupingModel(mod.name, noGeometry);
		if (nullSpacer)
		{
			nameToTransform[mod.name] = parentObj;
			continue;
		}

		MFnTransform grp;
		MObject tfObj = grp.create(parentObj);
		MString nodeName(mod.name.c_str());
		if (nodeName.length() == 0)
			nodeName = "model";
		grp.setName(nodeName);

		const Vec3 mt = toMayaPos(mod.translation);
		grp.setTranslation(MVector(mt.x, mt.y, mt.z), MSpace::kTransform);
		MQuaternion mq = toMayaRot(mod.rot[0], mod.rot[1], mod.rot[2], mod.rot[3]);
		grp.setRotation(mq);
		{
			const double s[3] = {static_cast<double>(sanitizeScaleComponent(mod.scale.x)),
			                     static_cast<double>(sanitizeScaleComponent(mod.scale.y)),
			                     static_cast<double>(sanitizeScaleComponent(mod.scale.z))};
			grp.setScale(s);
		}

		nameToTransform[mod.name] = tfObj;

		for (size_t si = 0; si < mod.segments.size(); ++si)
		{
			SegmentBf const& seg = mod.segments[si];
			MFloatPointArray points;
			MIntArray polyCounts;
			MIntArray polyConnects;
			MFloatArray uArray;
			MFloatArray vArray;

			const int nv = static_cast<int>(seg.positions.size());
			for (int vi = 0; vi < nv; ++vi)
			{
				const Vec3 pm = toMayaPos(seg.positions[static_cast<size_t>(vi)]);
				points.append(pm.x, pm.y, pm.z);
				if (vi < static_cast<int>(seg.texcoords.size()))
				{
					uArray.append(seg.texcoords[static_cast<size_t>(vi)].u);
					vArray.append(seg.texcoords[static_cast<size_t>(vi)].v);
				}
				else
				{
					uArray.append(0.f);
					vArray.append(0.f);
				}
			}

			const int ntri = static_cast<int>(seg.triangles.size() / 3);
			for (int ti = 0; ti < ntri; ++ti)
			{
				const uint32_t i0 = seg.triangles[static_cast<size_t>(ti * 3 + 0)];
				const uint32_t i1 = seg.triangles[static_cast<size_t>(ti * 3 + 1)];
				const uint32_t i2 = seg.triangles[static_cast<size_t>(ti * 3 + 2)];
				if (i0 >= static_cast<uint32_t>(nv) || i1 >= static_cast<uint32_t>(nv) || i2 >= static_cast<uint32_t>(nv))
					continue;
				polyCounts.append(3);
				polyConnects.append(static_cast<int>(i0));
				polyConnects.append(static_cast<int>(i1));
				polyConnects.append(static_cast<int>(i2));
			}

			if (polyCounts.length() == 0)
				continue;

			MFnMesh meshFn;
			MStatus meshSt;
			MIntArray edgeArray;
			MObject meshObj = meshFn.create(points, edgeArray, polyCounts, polyConnects, tfObj, &meshSt);
			if (!meshSt)
				continue;
			MString meshName = nodeName + "_mesh";
			if (mod.segments.size() > 1)
				meshName = meshName + static_cast<int>(si);
			meshFn.setName(meshName);

			{
				MString sgCmd = "sets -e -forceElement initialShadingGroup \"" + meshFn.fullPathName() + "\"";
				MGlobal::executeCommand(sgCmd);
			}

			MString doubleSidedCmd = "setAttr \"" + meshFn.fullPathName() + ".doubleSided\" 1";
			MGlobal::executeCommand(doubleSidedCmd);

			if (uArray.length() == points.length() && vArray.length() == points.length())
			{
				MIntArray uvCounts, uvIds;
				const int numPolygons = meshFn.numPolygons();
				for (int pi = 0; pi < numPolygons; ++pi)
				{
					MIntArray vtx;
					meshFn.getPolygonVertices(pi, vtx);
					uvCounts.append(static_cast<int>(vtx.length()));
					for (unsigned j = 0; j < vtx.length(); ++j)
					{
						const int vid = vtx[j];
						uvIds.append((vid >= 0 && vid < static_cast<int>(uArray.length())) ? vid : 0);
					}
				}
				MayaSceneBuilder::applyMap1Uvs(meshFn, uArray, vArray, uvCounts, uvIds);
			}

			if (static_cast<int>(seg.texcoords1.size()) == nv && nv > 0)
			{
				MFloatArray u1;
				MFloatArray v1;
				for (int vi = 0; vi < nv; ++vi)
				{
					u1.append(seg.texcoords1[static_cast<size_t>(vi)].u);
					v1.append(seg.texcoords1[static_cast<size_t>(vi)].v);
				}
				MString map2("map2");
				meshFn.setUVs(u1, v1, &map2);
				const int numPolygons2 = meshFn.numPolygons();
				for (int pi = 0; pi < numPolygons2; ++pi)
				{
					MIntArray vtx;
					meshFn.getPolygonVertices(pi, vtx);
					for (unsigned j = 0; j < vtx.length(); ++j)
					{
						const int vid = vtx[j];
						if (vid >= 0 && vid < u1.length())
							meshFn.assignUV(pi, static_cast<int>(j), vid, &map2);
					}
				}
			}

			if (static_cast<int>(seg.vertexColors.size()) == nv && nv > 0)
			{
				MColorArray carr;
				for (int ci = 0; ci < nv; ++ci)
				{
					Vec4 const& vc = seg.vertexColors[static_cast<size_t>(ci)];
					carr.append(MColor(vc.r, vc.g, vc.b, vc.a));
				}
				MString colorSetName = meshName + "_CLRL";
				MStatus csSt;
				meshFn.createColorSetWithName(colorSetName, nullptr, nullptr, &csSt);
				if (csSt == MS::kSuccess)
					meshFn.setColors(carr, &colorSetName);
			}
		}
	}

	return MS::kSuccess;
}

static bool fileStartsWithFormFourCc(char const* path)
{
	FILE* f = nullptr;
#ifdef _WIN32
	fopen_s(&f, path, "rb");
#else
	f = std::fopen(path, "rb");
#endif
	if (!f)
		return false;
	char buf[4] = {};
	const size_t n = std::fread(buf, 1, 4, f);
	std::fclose(f);
	return n == 4 && std::memcmp(buf, "FORM", 4) == 0;
}

} // namespace

void* SwbfMshTranslator::creator()
{
	return new SwbfMshTranslator();
}

MString SwbfMshTranslator::defaultExtension() const
{
	return "msh";
}

MString SwbfMshTranslator::filter() const
{
	return MString(swg_translator::kFilterSwbfMsh);
}

MPxFileTranslator::MFileKind SwbfMshTranslator::identifyFile(const MFileObject& fileName, const char* /*buffer*/, short /*size*/) const
{
	const std::string pathStr = MayaUtility::fileObjectPathForIdentify(fileName);
	const int nameLength = static_cast<int>(pathStr.size());
	if (nameLength <= 4)
		return kNotMyFileType;
	const char* ext = pathStr.c_str() + nameLength - 4;
	if (strcasecmp(ext, ".msh") != 0)
		return kNotMyFileType;
	if (fileStartsWithFormFourCc(pathStr.c_str()))
		return kNotMyFileType;
	return kCouldBeMyFileType;
}

MStatus SwbfMshTranslator::reader(const MFileObject& file, const MString& /*optionsString*/, MPxFileTranslator::FileAccessMode /*mode*/)
{
	const char* path = file.expandedFullName().asChar();
	std::vector<uint8_t> buf;
	if (!loadEntireFile(path, buf))
	{
		MGlobal::displayError(MString("[SwbfMsh] Could not read file or file too large (max ") + static_cast<int>(kMaxFileBytes / (1024 * 1024))
		                        + " MB): " + path);
		return MS::kFailure;
	}

	std::vector<MaterialBf> materials;
	std::vector<ModelBf> models;
	if (!parseSwbfMshBuffer(buf, materials, models))
	{
		MGlobal::displayError(MString("[SwbfMsh] Failed to parse Battlefront .msh (expected HEDR/MSH2). Not SWG IFF — use SwgMsh for Galaxies .msh): ")
		                        + path);
		return MS::kFailure;
	}

	if (models.empty())
	{
		MGlobal::displayWarning(MString("[SwbfMsh] No MODL geometry found (animation/skeleton-only file?): ") + path);
		return MS::kSuccess;
	}

	MStatus st = buildMeshesForModels(models, file.rawName());
	if (!st)
		MGlobal::displayError("[SwbfMsh] Failed to build Maya meshes.");
	return st;
}

MStatus SwbfMshTranslator::writer(const MFileObject& /*file*/, const MString& /*optionsString*/, MPxFileTranslator::FileAccessMode /*mode*/)
{
	return MS::kFailure;
}
