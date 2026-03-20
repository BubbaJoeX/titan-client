#ifndef SWG_ASSET_DISSECTOR_H
#define SWG_ASSET_DISSECTOR_H

#include <maya/MPxCommand.h>

class SwgAssetDissector : public MPxCommand
{
public:
    SwgAssetDissector() = default;
    ~SwgAssetDissector() override = default;

    static void* creator();
    static MStatus doIt(const MArgList& args);

private:
    static const char* s_windowName;
};

#endif
