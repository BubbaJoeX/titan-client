#ifndef SWGMAYAEDITOR_MGN_H
#define SWGMAYAEDITOR_MGN_H

#include "Tag.h"
#include "Iff.h"
#include "CrcLowerString.h"
#include "MeshConstructionHelper.h"


#include <maya/MPxFileTranslator.h>

class MgnTranslator : public MPxFileTranslator
{
public:
    
    static constexpr Tag TAG_OITL = TAG(O,I,T,L);
    static constexpr Tag TAG_ITL = TAG3(I,T,L);
    
    static const int ms_blendTargetNameSize;
    
    MgnTranslator () = default;
    ~MgnTranslator () override = default;
    
    // true because we read/import this type
    [[nodiscard]] bool haveReadMethod() const override { return true; }
    
    // true because we write/export this type
    [[nodiscard]] bool haveWriteMethod() const override { return true; }
    
    // true because we support both import and open operations
    [[nodiscard]] bool canBeOpened() const override { return true; }
    
    // false to use maya file referencing
    [[nodiscard]] bool haveReferenceMethod() const override { return false; }
    
    // whether we support name spaces
    [[nodiscard]] bool haveNamespaceSupport()    const override { return true; }
    
    // creates instances of the translator
    static void* creator();
    
    // the extension of the file
    [[nodiscard]] MString defaultExtension () const override;
    [[nodiscard]] MString filter () const override;
    
    // identify/read/write methods
    
    MFileKind identifyFile (const MFileObject& fileName, const char* buffer, short size) const override;
    MStatus reader (const MFileObject& file, const MString& optionsString, MPxFileTranslator::FileAccessMode mode) override;
    MStatus writer (const MFileObject& file, const MString& optionsString, MPxFileTranslator::FileAccessMode mode) override;
    
    struct Dot3Vector;
    struct BlendVector;
    class Hardpoint;
    class IndexedTriListPrimitive;
    class OccludedIndexedTriListPrimitive;
    class PerShaderData;
    class BlendTarget;


private:
    static MString const magic;
    
};

#endif //SWGMAYAEDITOR_MGN_H
