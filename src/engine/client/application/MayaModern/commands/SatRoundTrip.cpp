#include "SatRoundTrip.h"

#include "Iff.h"

#include <maya/MFnDependencyNode.h>
#include <maya/MFnTransform.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MGlobal.h>
#include <maya/MPlug.h>
#include <maya/MString.h>

#include <cctype>
#include <cstdio>
#include <sstream>

namespace
{
	const Tag TAG_SMAT = TAG(S, M, A, T);
	const Tag TAG_MSGN = TAG(M, S, G, N);
	const Tag TAG_SKTI = TAG(S, K, T, I);
	const Tag TAG_LATX = TAG(L, A, T, X);
	const Tag TAG_LDTB = TAG(L, D, T, B);
	const Tag TAG_SFSK = TAG(S, F, S, K);
	const Tag TAG_APAG = TAG(A, P, A, G);

	std::string tagToAscii(Tag t)
	{
		char b[5];
		ConvertTagToString(t, b);
		return std::string(b, 4);
	}

	std::string escLine(std::string s)
	{
		std::string o;
		o.reserve(s.size() + 8);
		for (char c : s)
		{
			if (c == '\\')
				o += "\\\\";
			else if (c == '\n')
				o += "\\n";
			else if (c == '\r')
				o += "\\r";
			else
				o += c;
		}
		return o;
	}

	std::string unescLine(std::string s)
	{
		std::string o;
		for (size_t i = 0; i < s.size(); ++i)
		{
			if (s[i] == '\\' && i + 1 < s.size())
			{
				if (s[i + 1] == 'n')
				{
					o += '\n';
					++i;
				}
				else if (s[i + 1] == 'r')
				{
					o += '\r';
					++i;
				}
				else if (s[i + 1] == '\\')
				{
					o += '\\';
					++i;
				}
				else
					o += s[i];
			}
			else
				o += s[i];
		}
		return o;
	}

	static bool hexDigit(char c, int& v)
	{
		if (c >= '0' && c <= '9')
		{
			v = c - '0';
			return true;
		}
		if (c >= 'a' && c <= 'f')
		{
			v = 10 + (c - 'a');
			return true;
		}
		if (c >= 'A' && c <= 'F')
		{
			v = 10 + (c - 'A');
			return true;
		}
		return false;
	}

	std::string bytesToHex(const std::vector<std::uint8_t>& b)
	{
		static const char* hx = "0123456789abcdef";
		std::string o;
		o.resize(b.size() * 2);
		for (size_t i = 0; i < b.size(); ++i)
		{
			o[i * 2] = hx[b[i] >> 4];
			o[i * 2 + 1] = hx[b[i] & 0xf];
		}
		return o;
	}

	bool hexToBytes(const std::string& hex, std::vector<std::uint8_t>& out)
	{
		if (hex.size() % 2)
			return false;
		out.clear();
		out.reserve(hex.size() / 2);
		for (size_t i = 0; i < hex.size(); i += 2)
		{
			int hi, lo;
			if (!hexDigit(hex[i], hi) || !hexDigit(hex[i + 1], lo))
				return false;
			out.push_back(static_cast<std::uint8_t>((hi << 4) | lo));
		}
		return true;
	}

	Tag asciiToTag4(const std::string& s)
	{
		if (s.size() != 4)
			return 0;
		// Pack big-endian style like client TAG macro
		const auto u8 = [](char c) -> uint32 { return static_cast<uint32>(static_cast<unsigned char>(c)); };
		return (u8(s[0]) << 24) | (u8(s[1]) << 16) | (u8(s[2]) << 8) | u8(s[3]);
	}

	/** Maya node name: alnum + underscore only; keep leading digits (e.g. 4lom) to match asset basename. */
	std::string sanitizeMayaName(std::string s)
	{
		for (char& c : s)
		{
			if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_'))
				c = '_';
		}
		return s;
	}
} // namespace

namespace sat_round_trip
{

namespace
{
	void appendCurrentFormContents(Iff& readIff, Iff& buildIff)
	{
		while (!readIff.atEndOfForm())
		{
			if (readIff.isCurrentForm())
			{
				const Tag innerTag = readIff.getCurrentName();
				buildIff.insertForm(innerTag);
				readIff.enterForm();
				appendCurrentFormContents(readIff, buildIff);
				readIff.exitForm();
				buildIff.exitForm(innerTag);
			}
			else
			{
				const Tag t = readIff.getCurrentName();
				readIff.enterChunk(t);
				const int n = readIff.getChunkLengthLeft(1);
				std::vector<std::uint8_t> buf(static_cast<size_t>(std::max(0, n)));
				if (n > 0)
					readIff.read_uint8(n, buf.data());
				readIff.exitChunk(t);
				buildIff.insertChunk(t);
				if (!buf.empty())
					buildIff.insertChunkData(buf.data(), static_cast<int>(buf.size()));
				buildIff.exitChunk(t);
			}
		}
	}
} // namespace

bool parseTailAfterSkti(Iff& iff, SatRoundTripData& out)
{
	if (iff.enterChunk(TAG_LATX, true))
	{
		const int count = static_cast<int>(iff.read_int16());
		for (int i = 0; i < count; ++i)
		{
			const std::string k = iff.read_stdstring();
			const std::string v = iff.read_stdstring();
			out.latx.emplace_back(k, v);
		}
		iff.exitChunk(TAG_LATX);
	}

	if (!iff.atEndOfForm() && iff.getCurrentName() == TAG_LDTB)
	{
		iff.enterForm(TAG_LDTB);
		iff.enterForm(TAG_0000);
		iff.enterChunk(TAG_INFO);
		const int n = static_cast<int>(iff.read_int16());
		for (int i = 0; i < n; ++i)
		{
			const float mn = iff.read_float();
			const float mx = iff.read_float();
			out.lodDistances.emplace_back(mn, mx);
		}
		iff.exitChunk(TAG_INFO);
		iff.exitForm(TAG_0000);
		iff.exitForm(TAG_LDTB);
	}

	if (iff.enterChunk(TAG_SFSK, true))
	{
		out.hasSoftSkinningChunk = true;
		out.softSkinning = iff.read_uint8();
		iff.exitChunk(TAG_SFSK);
	}

	if (iff.enterChunk(TAG_APAG, true))
	{
		out.hasApagChunk = true;
		out.apag = iff.read_uint8();
		iff.exitChunk(TAG_APAG);
	}

	while (!iff.atEndOfForm())
	{
		if (iff.isCurrentForm())
		{
			const Tag formTag = iff.getCurrentName();
			iff.enterForm();
			Iff sub(65536, true, true);
			sub.insertForm(formTag);
			appendCurrentFormContents(iff, sub);
			iff.exitForm();
			sub.exitForm(formTag);
			SatTrailingItem it;
			it.isForm = true;
			it.tag = formTag;
			const byte* raw = sub.getRawData();
			const int sz = sub.getRawDataSize();
			if (raw && sz > 0)
				it.payload.assign(raw, raw + sz);
			out.trailing.push_back(std::move(it));
			continue;
		}
		const Tag t = iff.getCurrentName();
		iff.enterChunk(t);
		const int n = iff.getChunkLengthLeft(1);
		std::vector<std::uint8_t> buf(static_cast<size_t>(std::max(0, n)));
		if (n > 0)
			iff.read_uint8(n, buf.data());
		iff.exitChunk(t);
		SatTrailingItem it;
		it.isForm = false;
		it.tag = t;
		it.payload = std::move(buf);
		out.trailing.push_back(std::move(it));
	}
	return true;
}

static Tag versionStringToTag(const std::string& v)
{
	if (v == "0001")
		return TAG_0001;
	if (v == "0002")
		return TAG_0002;
	if (v == "0003")
		return TAG_0003;
	return TAG_0003;
}

bool writeSmatFile(const char* filePath, const SatRoundTripData& d)
{
	Iff iff(65536, true, true);
	const Tag verTag = versionStringToTag(d.versionTag);

	iff.insertForm(TAG_SMAT);
	iff.insertForm(verTag);

	iff.insertChunk(TAG_INFO);
	if (verTag == TAG_0001)
	{
		iff.insertChunkData(static_cast<int32>(static_cast<int>(d.meshGenerators.size())));
		iff.insertChunkData(static_cast<int32>(static_cast<int>(d.skeletonTemplates.size())));
		iff.insertChunkString(d.infoExtraString.c_str());
	}
	else if (verTag == TAG_0002)
	{
		iff.insertChunkData(static_cast<int32>(static_cast<int>(d.meshGenerators.size())));
		iff.insertChunkData(static_cast<int32>(static_cast<int>(d.skeletonTemplates.size())));
		iff.insertChunkString(d.infoExtraString.c_str());
	}
	else
	{
		iff.insertChunkData(static_cast<int32>(static_cast<int>(d.meshGenerators.size())));
		iff.insertChunkData(static_cast<int32>(static_cast<int>(d.skeletonTemplates.size())));
		const char cc = d.createAnimationController ? 1 : 0;
		iff.insertChunkData(cc);
	}
	iff.exitChunk(TAG_INFO);

	iff.insertChunk(TAG_MSGN);
	for (const std::string& m : d.meshGenerators)
		iff.insertChunkString(m.c_str());
	iff.exitChunk(TAG_MSGN);

	iff.insertChunk(TAG_SKTI);
	for (const auto& pr : d.skeletonTemplates)
	{
		iff.insertChunkString(pr.first.c_str());
		iff.insertChunkString(pr.second.c_str());
	}
	iff.exitChunk(TAG_SKTI);

	if (!d.latx.empty())
	{
		iff.insertChunk(TAG_LATX);
		iff.insertChunkData(static_cast<int16>(static_cast<int>(d.latx.size())));
		for (const auto& pr : d.latx)
		{
			iff.insertChunkString(pr.first.c_str());
			iff.insertChunkString(pr.second.c_str());
		}
		iff.exitChunk(TAG_LATX);
	}

	if (!d.lodDistances.empty())
	{
		iff.insertForm(TAG_LDTB);
		iff.insertForm(TAG_0000);
		iff.insertChunk(TAG_INFO);
		iff.insertChunkData(static_cast<int16>(static_cast<int>(d.lodDistances.size())));
		for (const auto& band : d.lodDistances)
		{
			iff.insertChunkData(band.first);
			iff.insertChunkData(band.second);
		}
		iff.exitChunk(TAG_INFO);
		iff.exitForm(TAG_0000);
		iff.exitForm(TAG_LDTB);
	}

	if (d.hasSoftSkinningChunk)
	{
		iff.insertChunk(TAG_SFSK);
		iff.insertChunkData(d.softSkinning);
		iff.exitChunk(TAG_SFSK);
	}

	if (d.hasApagChunk)
	{
		iff.insertChunk(TAG_APAG);
		iff.insertChunkData(d.apag);
		iff.exitChunk(TAG_APAG);
	}

	for (const SatTrailingItem& it : d.trailing)
	{
		if (it.isForm)
		{
			if (!it.payload.empty())
			{
				Iff emb(static_cast<int>(it.payload.size()), reinterpret_cast<const byte*>(it.payload.data()), false);
				iff.insertIff(&emb);
			}
		}
		else
		{
			iff.insertChunk(it.tag);
			if (!it.payload.empty())
				iff.insertChunkData(it.payload.data(), static_cast<int>(it.payload.size()));
			iff.exitChunk(it.tag);
		}
	}

	iff.exitForm(verTag);
	iff.exitForm(TAG_SMAT);

	return iff.write(filePath, false);
}

std::string serializePayload(const SatRoundTripData& d)
{
	std::ostringstream os;
	os << "SRT1\n";
	os << "ver " << d.versionTag << "\n";
	os << "cc " << (d.createAnimationController ? 1 : 0) << "\n";
	os << "ix " << escLine(d.infoExtraString) << "\n";
	os << "mesh " << d.meshGenerators.size() << "\n";
	for (const auto& s : d.meshGenerators)
		os << escLine(s) << "\n";
	os << "skel " << d.skeletonTemplates.size() << "\n";
	for (const auto& pr : d.skeletonTemplates)
		os << escLine(pr.first) << "\t" << escLine(pr.second) << "\n";
	os << "latx " << d.latx.size() << "\n";
	for (const auto& pr : d.latx)
		os << escLine(pr.first) << "\t" << escLine(pr.second) << "\n";
	os << "ldtb " << d.lodDistances.size() << "\n";
	for (const auto& pr : d.lodDistances)
		os << pr.first << " " << pr.second << "\n";
	os << "sfsk " << (d.hasSoftSkinningChunk ? 1 : 0) << " " << static_cast<int>(d.softSkinning) << "\n";
	os << "apag " << (d.hasApagChunk ? 1 : 0) << " " << static_cast<int>(d.apag) << "\n";
	os << "trail " << d.trailing.size() << "\n";
	for (const auto& it : d.trailing)
	{
		os << (it.isForm ? "f " : "c ") << tagToAscii(it.tag) << "\n";
		os << bytesToHex(it.payload) << "\n";
	}
	os << "end\n";
	return os.str();
}

bool deserializePayload(const std::string& raw, SatRoundTripData& d)
{
	d = SatRoundTripData{};
	std::istringstream is(raw);
	std::string line;
	if (!std::getline(is, line) || line != "SRT1")
		return false;
	while (std::getline(is, line))
	{
		if (!line.empty() && line.back() == '\r')
			line.pop_back();
		if (line == "end")
			return true;
		std::istringstream ls(line);
		std::string key;
		ls >> key;
		if (key == "ver")
		{
			ls >> d.versionTag;
		}
		else if (key == "cc")
		{
			int v;
			ls >> v;
			d.createAnimationController = (v != 0);
		}
		else if (key == "ix")
		{
			std::string rest;
			std::getline(ls, rest);
			if (!rest.empty() && rest[0] == ' ')
				rest = rest.substr(1);
			d.infoExtraString = unescLine(rest);
		}
		else if (key == "mesh")
		{
			size_t n;
			ls >> n;
			d.meshGenerators.resize(n);
			for (size_t i = 0; i < n; ++i)
			{
				if (!std::getline(is, line))
					return false;
				if (!line.empty() && line.back() == '\r')
					line.pop_back();
				d.meshGenerators[i] = unescLine(line);
			}
		}
		else if (key == "skel")
		{
			size_t n;
			ls >> n;
			d.skeletonTemplates.resize(n);
			for (size_t i = 0; i < n; ++i)
			{
				if (!std::getline(is, line))
					return false;
				if (!line.empty() && line.back() == '\r')
					line.pop_back();
				const size_t tab = line.find('\t');
				if (tab == std::string::npos)
					return false;
				d.skeletonTemplates[i] = {unescLine(line.substr(0, tab)), unescLine(line.substr(tab + 1))};
			}
		}
		else if (key == "latx")
		{
			size_t n;
			ls >> n;
			d.latx.resize(n);
			for (size_t i = 0; i < n; ++i)
			{
				if (!std::getline(is, line))
					return false;
				if (!line.empty() && line.back() == '\r')
					line.pop_back();
				const size_t tab = line.find('\t');
				if (tab == std::string::npos)
					return false;
				d.latx[i] = {unescLine(line.substr(0, tab)), unescLine(line.substr(tab + 1))};
			}
		}
		else if (key == "ldtb")
		{
			size_t n;
			ls >> n;
			d.lodDistances.resize(n);
			for (size_t i = 0; i < n; ++i)
			{
				if (!std::getline(is, line))
					return false;
				if (!line.empty() && line.back() == '\r')
					line.pop_back();
				float a, b;
				if (std::sscanf(line.c_str(), "%f %f", &a, &b) != 2)
					return false;
				d.lodDistances[i] = {a, b};
			}
		}
		else if (key == "sfsk")
		{
			int h, v;
			ls >> h >> v;
			d.hasSoftSkinningChunk = (h != 0);
			d.softSkinning = static_cast<std::uint8_t>(v);
		}
		else if (key == "apag")
		{
			int h, v;
			ls >> h >> v;
			d.hasApagChunk = (h != 0);
			d.apag = static_cast<std::uint8_t>(v);
		}
		else if (key == "trail")
		{
			size_t n;
			ls >> n;
			d.trailing.resize(n);
			for (size_t i = 0; i < n; ++i)
			{
				if (!std::getline(is, line))
					return false;
				if (!line.empty() && line.back() == '\r')
					line.pop_back();
				std::istringstream ws(line);
				std::string kind;
				std::string tagStr;
				ws >> kind >> tagStr;
				if (tagStr.size() != 4)
					return false;
				d.trailing[i].isForm = (kind == "f");
				d.trailing[i].tag = asciiToTag4(tagStr);
				if (!std::getline(is, line))
					return false;
				if (!line.empty() && line.back() == '\r')
					line.pop_back();
				if (!hexToBytes(line, d.trailing[i].payload))
					return false;
			}
		}
		else if (key == "opaque")
		{
			size_t n;
			ls >> n;
			d.trailing.resize(n);
			for (size_t i = 0; i < n; ++i)
			{
				if (!std::getline(is, line))
					return false;
				if (!line.empty() && line.back() == '\r')
					line.pop_back();
				if (line.size() != 4)
					return false;
				d.trailing[i].isForm = false;
				d.trailing[i].tag = asciiToTag4(line);
				if (!std::getline(is, line))
					return false;
				if (!line.empty() && line.back() == '\r')
					line.pop_back();
				if (!hexToBytes(line, d.trailing[i].payload))
					return false;
			}
		}
	}
	return false;
}

bool createMetadataNode(const std::string& mayaBasename, const std::string& payloadUtf8)
{
	MStatus st;
	MFnTransform fn;
	const MObject transformObj = fn.create(MObject::kNullObj, &st);
	if (!st)
		return false;
	const std::string nodeName = sanitizeMayaName(mayaBasename);
	if (nodeName.empty())
		return false;
	fn.setName(MString(nodeName.c_str()), &st);
	if (!st)
		return false;

	MFnDependencyNode dep(transformObj);
	MFnTypedAttribute tAttr;
	const MObject attrObj = tAttr.create("swgSmatRoundTrip", "ssrt", MFnData::kString, MObject::kNullObj, &st);
	if (!st)
		return false;
	tAttr.setStorable(true);
	tAttr.setKeyable(false);
	st = dep.addAttribute(attrObj);
	if (!st)
		return false;

	MPlug plug = dep.findPlug("swgSmatRoundTrip", true, &st);
	if (!st)
		return false;
	plug.setString(MString(payloadUtf8.c_str()));

	MGlobal::displayInfo(MString("SWG SAT: stored round-trip metadata on ") + MString(nodeName.c_str()) + MString(" (attribute swgSmatRoundTrip)."));
	return true;
}

} // namespace sat_round_trip
