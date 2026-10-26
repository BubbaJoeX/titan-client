#ifndef SWG_MOCAP_SUITE_H
#define SWG_MOCAP_SUITE_H

#include <maya/MPxCommand.h>

class SwgMocapSuite : public MPxCommand
{
public:
    SwgMocapSuite() = default;
    ~SwgMocapSuite() override = default;

    static void* creator();
    MStatus doIt(const MArgList& args) override;
};

#endif
