#ifndef SWGMAYAEDITOR_SWBF_MSH_H
#define SWGMAYAEDITOR_SWBF_MSH_H

#include <maya/MPxFileTranslator.h>
#include <maya/MString.h>

/**
 * Import-only translator for Star Wars Battlefront (2004) / Battlefront II (2005) toolchain
 * chunked ".msh" mesh files — NOT Star Wars Galaxies IFF ".msh".
 *
 * Format reference: community tooling aligned with
 * https://github.com/PrismaticFlower/SWBF-msh-Blender-IO (chunked HEDR / MSH2 / MODL / SEGM).
 */
class SwbfMshTranslator : public MPxFileTranslator
{
public:
	SwbfMshTranslator() = default;
	~SwbfMshTranslator() override = default;

	[[nodiscard]] bool haveReadMethod() const override { return true; }
	[[nodiscard]] bool haveWriteMethod() const override { return false; }
	[[nodiscard]] bool canBeOpened() const override { return true; }
	[[nodiscard]] bool haveReferenceMethod() const override { return false; }
	[[nodiscard]] bool haveNamespaceSupport() const override { return true; }

	static void* creator();

	[[nodiscard]] MString defaultExtension() const override;
	[[nodiscard]] MString filter() const override;

	MPxFileTranslator::MFileKind identifyFile(const MFileObject& fileName, const char* buffer, short size) const override;
	MStatus reader(const MFileObject& file, const MString& optionsString, MPxFileTranslator::FileAccessMode mode) override;
	MStatus writer(const MFileObject& file, const MString& optionsString, MPxFileTranslator::FileAccessMode mode) override;
};

#endif
