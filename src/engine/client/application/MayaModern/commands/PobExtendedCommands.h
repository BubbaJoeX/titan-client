#ifndef SWGMAYAEDITOR_POBEXTENDEDCOMMANDS_H
#define SWGMAYAEDITOR_POBEXTENDEDCOMMANDS_H

#include <maya/MPxCommand.h>

class ValidatePob : public MPxCommand
{
public:
    static void* creator();
    MStatus doIt(const MArgList& args) override;
};

class ConnectPobCells : public MPxCommand
{
public:
    static void* creator();
    MStatus doIt(const MArgList& args) override;
};

class LayoutPobCells : public MPxCommand
{
public:
    static void* creator();
    MStatus doIt(const MArgList& args) override;
};

class DuplicatePobCell : public MPxCommand
{
public:
    static void* creator();
    MStatus doIt(const MArgList& args) override;
};

#endif
