#ifndef SWGMAYAEDITOR_LMG_H
#define SWGMAYAEDITOR_LMG_H

#include <maya/MPxFileTranslator.h>

/** Skeletal mesh generator .lmg (SKMG) — delegates to importSkeletalMesh. */
class LmgTranslator : public MPxFileTranslator
{
public:
	LmgTranslator() = default;
	~LmgTranslator() override = default;

	[[nodiscard]] bool haveReadMethod() const override { return true; }
	[[nodiscard]] bool haveWriteMethod() const override { return false; }
	[[nodiscard]] bool canBeOpened() const override { return true; }
	[[nodiscard]] bool haveReferenceMethod() const override { return false; }
	[[nodiscard]] bool haveNamespaceSupport() const override { return true; }

	static void* creator();
	[[nodiscard]] MString defaultExtension() const override;
	[[nodiscard]] MString filter() const override;
	MFileKind identifyFile(const MFileObject& fileName, const char* buffer, short size) const override;
	MStatus reader(const MFileObject& file, const MString& optionsString, MPxFileTranslator::FileAccessMode mode) override;
};

#endif
