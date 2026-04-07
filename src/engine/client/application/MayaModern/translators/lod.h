#ifndef SWGMAYAEDITOR_LOD_H
#define SWGMAYAEDITOR_LOD_H

#include <maya/MPxFileTranslator.h>

/** Mesh / appearance LOD (.lod) — delegates to importLodMesh. */
class LodTranslator : public MPxFileTranslator
{
public:
	LodTranslator() = default;
	~LodTranslator() override = default;

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
