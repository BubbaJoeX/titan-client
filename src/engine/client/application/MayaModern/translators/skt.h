#ifndef SWGMAYAEDITOR_SKT_H
#define SWGMAYAEDITOR_SKT_H

#include "Tag.h"

#include <maya/MPxFileTranslator.h>
#include <maya/MQuaternion.h>

class SktTranslator : public MPxFileTranslator
{
public:
    
    const Tag TAG_SLOD = TAG(S,L,O,D);
    const Tag TAG_SKTM = TAG(S,K,T,M);
    const Tag TAG_PRNT = TAG(P,R,N,T);
    const Tag TAG_RPRE = TAG(R,P,R,E);
    const Tag TAG_RPST = TAG(R,P,S,T);
    const Tag TAG_BPTR = TAG(B,P,T,R);
    const Tag TAG_BPRO = TAG(B,P,R,O);
    const Tag TAG_JROR = TAG(J,R,O,R);
    
    SktTranslator () = default;
    ~SktTranslator () override = default;
    
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

private:
    static MString const magic;
    
    static MEulerRotation getMayaEulerFromSwgQuaternion(const MQuaternion& q);
    
};

#endif //SWGMAYAEDITOR_SKT_H
