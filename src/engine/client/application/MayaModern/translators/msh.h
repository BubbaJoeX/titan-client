#ifndef SWGMAYAEDITOR_MSH_H
#define SWGMAYAEDITOR_MSH_H

#include "Tag.h"

#include <maya/MPxFileTranslator.h>

class MshTranslator : public MPxFileTranslator
{
public:
    const Tag TAG_MESH = TAG(M,E,S,H);
    const Tag TAG_NAME = TAG(N,A,M,E);
    const Tag TAG_VTXA = TAG(V,T,X,A);
    const Tag TAG_INDX = TAG(I,N,D,X);
    const Tag TAG_SIDX = TAG(S,I,D,X);
    const Tag TAG_SPS = TAG3(S,P,S);
    const Tag TAG_CNT = TAG3(C,N,T);
    
    MshTranslator () = default;
    ~MshTranslator () override = default;
    
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

    /** Load .msh file directly (bypasses MFileIO). Returns root transform path.
     *  If parentPath is non-empty, creates mesh under that parent (avoids post-parent command). */
    static MStatus createMeshFromMsh(const char* mshPath, MString& outRootPath, const MString& parentPath = MString());

private:
    static MString const magic;
};

#endif //SWGMAYAEDITOR_MSH_H
