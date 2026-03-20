#ifndef SWGMAYAEDITOR_FLR_H
#define SWGMAYAEDITOR_FLR_H


#include "Tag.H"

#include <maya/MPxFileTranslator.h>

class FlrTranslator : public MPxFileTranslator
    {
public:
    
    const Tag TAG_VERT = TAG(V,E,R,T);
    const Tag TAG_TRIS = TAG(T,R,I,S);
    const Tag TAG_FLOR = TAG(F,L,O,R);
    const Tag TAG_PNOD = TAG(P,N,O,D);
    const Tag TAG_PEDG = TAG(P,E,D,G);
    const Tag TAG_BTRE = TAG(B,T,R,E);
    const Tag TAG_BEDG = TAG(B,E,D,G);
    const Tag TAG_PGRF = TAG(P,G,R,F);

    FlrTranslator () = default;
    ~FlrTranslator () override = default;
    
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

    /** Load .flr file and create Maya mesh under parent. Returns mesh path. */
    static MStatus createMeshFromFlr(const char* flrPath, const char* meshName, MObject parentObj, MDagPath& outPath);

private:
    static MString const magic;
    };


#endif //SWGMAYAEDITOR_FLR_H
