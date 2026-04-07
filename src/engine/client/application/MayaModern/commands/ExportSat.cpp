#include "ExportSat.h"
#include "SatRoundTrip.h"

#include <maya/MArgList.h>
#include <maya/MFn.h>
#include <maya/MDagPath.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnDagNode.h>
#include <maya/MGlobal.h>
#include <maya/MItDependencyNodes.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MSelectionList.h>
#include <maya/MString.h>

#include <string>

namespace
{
	MObject findNodeWithPayload(const std::string& explicitName)
	{
		if (!explicitName.empty())
		{
			MSelectionList sel;
			if (MGlobal::getSelectionListByName(MString(explicitName.c_str()), sel) && sel.length() > 0)
			{
				MObject o;
				sel.getDependNode(0, o);
				return o;
			}
		}
		MSelectionList active;
		if (MGlobal::getActiveSelectionList(active))
		{
			for (unsigned i = 0; i < active.length(); ++i)
			{
				MDagPath dp;
				if (active.getDagPath(i, dp) != MS::kSuccess)
					continue;
				if (!dp.hasFn(MFn::kTransform))
					continue;
				MFnDependencyNode fn(dp.node());
				if (fn.hasAttribute("swgSmatRoundTrip"))
				{
					MPlug p = fn.findPlug("swgSmatRoundTrip", true);
					MString s;
					if (!p.isNull() && p.getValue(s) == MS::kSuccess && s.length() > 0)
						return dp.node();
				}
			}
		}
		MItDependencyNodes it(MFn::kTransform);
		for (; !it.isDone(); it.next())
		{
			MObject o = it.thisNode();
			MFnDependencyNode fn(o);
			if (!fn.hasAttribute("swgSmatRoundTrip"))
				continue;
			MPlug p = fn.findPlug("swgSmatRoundTrip", true);
			if (p.isNull())
				continue;
			MString s;
			if (p.getValue(s) == MS::kSuccess && s.length() > 0)
				return o;
		}
		return MObject::kNullObj;
	}
} // namespace

void* ExportSat::creator()
{
	return new ExportSat();
}

MStatus ExportSat::doIt(const MArgList& args)
{
	MStatus st;
	std::string outPath;
	std::string nodeName;

	for (unsigned i = 0; i < args.length(&st); ++i)
	{
		if (!st)
			return MS::kFailure;
		MString a = args.asString(i, &st);
		if (!st)
			return MS::kFailure;
		if (a == MString("-o") && i + 1 < args.length(&st))
		{
			outPath = args.asString(++i, &st).asChar();
		}
		else if (a == MString("-node") && i + 1 < args.length(&st))
		{
			nodeName = args.asString(++i, &st).asChar();
		}
	}

	if (outPath.empty())
	{
		MGlobal::displayError("exportSat: specify output path with -o \"path/to/file.sat\"");
		return MS::kFailure;
	}

	MObject node = findNodeWithPayload(nodeName);
	if (node.isNull())
	{
		MGlobal::displayError("exportSat: no transform with non-empty swgSmatRoundTrip attribute found (use importSat first, or -node with the SAT basename transform).");
		return MS::kFailure;
	}

	MFnDependencyNode fn(node);
	MPlug plug = fn.findPlug("swgSmatRoundTrip", true, &st);
	if (!st || plug.isNull())
	{
		MGlobal::displayError("exportSat: swgSmatRoundTrip plug missing.");
		return MS::kFailure;
	}
	MString payloadStr;
	st = plug.getValue(payloadStr);
	if (!st)
		return MS::kFailure;

	SatRoundTripData data;
	if (!sat_round_trip::deserializePayload(std::string(payloadStr.asChar()), data))
	{
		MGlobal::displayError("exportSat: failed to deserialize swgSmatRoundTrip payload.");
		return MS::kFailure;
	}

	if (!sat_round_trip::writeSmatFile(outPath.c_str(), data))
	{
		MGlobal::displayError(MString("exportSat: failed to write ") + outPath.c_str());
		return MS::kFailure;
	}

	MGlobal::displayInfo(MString("SWG SAT exported: ") + outPath.c_str());
	return MS::kSuccess;
}
