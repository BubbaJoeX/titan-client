#ifndef SWGMAYAEDITOR_ANS_H
#define SWGMAYAEDITOR_ANS_H

#include "Tag.H"
#include "Binary.h"
#include "CompressedQuaternion.h"
#include "Vector.h"

#include <maya/MPxFileTranslator.h>

class AnsTranslator : public MPxFileTranslator
    {
public:

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    struct RealKeyData
    {
    public:
        RealKeyData() : m_frameNumber(0), m_keyValue(0), m_oneOverDistanceToNextKeyframe(0) {}
        RealKeyData(float frameNumber, float keyValue) : m_frameNumber(frameNumber), m_keyValue(keyValue), m_oneOverDistanceToNextKeyframe(0) {}

    public:

        float  m_frameNumber;
        float  m_keyValue;
        float  m_oneOverDistanceToNextKeyframe;

    private:

    };

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    struct QuaternionKeyData
    {
    public:

        QuaternionKeyData() : m_frameNumber(0), m_rotation(0), m_oneOverDistanceToNextKeyframe(0) {}
        QuaternionKeyData(float frameNumber, const CompressedQuaternion &rotation) : m_frameNumber(frameNumber), m_rotation(rotation), m_oneOverDistanceToNextKeyframe(0) {}

    public:

        float                 m_frameNumber;
        CompressedQuaternion  m_rotation;
        float                 m_oneOverDistanceToNextKeyframe;

    private:
    };

    class RotationChannel;

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    struct VectorKeyData
    {
    public:

        VectorKeyData(float frameNumber, const Vector &vector);

    public:

        float   m_frameNumber;
        Vector  m_vector;
        float   m_oneOverDistanceToNextKeyframe;

    private:

        // disabled
        VectorKeyData();
    };

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    static constexpr Tag TAG_KFAT = TAG(K,F,A,T);
    static constexpr Tag TAG_CKAT = TAG(C,K,A,T);
    static constexpr Tag TAG_XFRM = TAG(X,F,R,M);
    static constexpr Tag TAG_XFIN = TAG(X,F,I,N);
    static constexpr Tag TAG_AROT = TAG(A,R,O,T);
    static constexpr Tag TAG_QCHN = TAG(Q,C,H,N);
    static constexpr Tag TAG_SROT = TAG(S,R,O,T);
    static constexpr Tag TAG_ATRN = TAG(A,T,R,N);
    static constexpr Tag TAG_CHNL = TAG(C,H,N,L);
    static constexpr Tag TAG_STRN = TAG(S,T,R,N);
    static constexpr Tag TAG_MSGS = TAG(M,S,G,S);
    static constexpr Tag TAG_MESG = TAG(M,E,S,G);
    static constexpr Tag TAG_LOCT = TAG(L,O,C,T);
    static constexpr Tag TAG_LOCR = TAG(L,O,C,R);
    static constexpr Tag TAG_QCNH = TAG(Q,C,N,H);
    
    // Animation message data
    struct AnimationMessage
    {
        std::string name;
        std::vector<int> signalFrameNumbers;
    };
    
    // Locomotion data
    struct LocomotionData
    {
        float averageTranslationSpeed = 0.0f;
        std::vector<VectorKeyData> translationKeys;
        std::vector<QuaternionKeyData> rotationKeys;
    };
    
    // ======================================================================
    
    enum // channel component flags
        {
        SATCCF_xRotation       = BINARY2(0000,0001),  // if set, this rotation animates; otherwise, it is static throughout the course of this skeletal animation
        SATCCF_yRotation       = BINARY2(0000,0010),
        SATCCF_zRotation       = BINARY2(0000,0100),
        SATCCF_rotationMask    = BINARY2(0000,0111),
        
        SATCCF_xTranslation    = BINARY2(0000,1000),  // if set, this translation animates; otherwise, it is static
        SATCCF_yTranslation    = BINARY2(0001,0000),
        SATCCF_zTranslation    = BINARY2(0010,0000),
        SATCCF_translationMask = BINARY2(0011,1000)
        
        };

    // ======================================================================
    
    AnsTranslator () = default;
    ~AnsTranslator () override = default;
    
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

    // soe animation stuff

    static void  calculateVectorDistanceToNextFrame(std::vector<VectorKeyData> &keyDataVector);
    static void  calculateQuaternionDistanceToNextFrame(std::vector<QuaternionKeyData> &keyDataVector);
    static void  calculateFloatDistanceToNextFrame(std::vector<RealKeyData> &keyDataVector);
    static bool shouldCompressKeyFrame(float const previousValue, float const nextValue);

private:

    class FullCompressedQuaternion;

    static MString const magic;
    };

#endif //SWGMAYAEDITOR_ANS_H
