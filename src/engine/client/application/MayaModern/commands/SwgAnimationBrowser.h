#ifndef SWG_ANIMATION_BROWSER_H
#define SWG_ANIMATION_BROWSER_H

#include <maya/MPxCommand.h>

class SwgAnimationBrowser : public MPxCommand
{
public:
    SwgAnimationBrowser() = default;
    ~SwgAnimationBrowser() override = default;

    static void* creator();
    MStatus doIt(const MArgList& args) override;
};

#endif
