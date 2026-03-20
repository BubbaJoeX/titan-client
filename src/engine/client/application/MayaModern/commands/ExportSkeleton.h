#ifndef SWGMAYAEDITOR_EXPORTSKELETON_H
#define SWGMAYAEDITOR_EXPORTSKELETON_H

#include <maya/MPxCommand.h>

class ExportSkeleton : public MPxCommand
{
public:
    static void* creator();
    MStatus doIt(const MArgList& args) override;

private:
    bool performSingleSkeletonExport(int bindPoseFrameNumber, const class MDagPath& targetDagPath, std::string& skeletonName, const std::string& outputPathOverride = {});
    bool addMayaJoint(class SkeletonTemplateWriter& writer, const class MDagPath& targetDagPath, int parentIndex);
};

#endif
