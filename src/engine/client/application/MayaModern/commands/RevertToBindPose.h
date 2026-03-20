#ifndef SWGMAYAEDITOR_REVERTTOBINDPOSE_H
#define SWGMAYAEDITOR_REVERTTOBINDPOSE_H

#include <maya/MPxCommand.h>

class RevertToBindPose : public MPxCommand
{
public:
    static void* creator();
    MStatus doIt(const MArgList& args) override;
};

#endif
