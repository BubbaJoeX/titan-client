// ======================================================================
//
// ImportShader.cpp
// Copyright 2006 Sony Online Entertainment, Inc.
// All Rights Reserved.
//
// ======================================================================

#include "FirstMayaExporter.h"
#include "ImportPathResolver.h"
#include "ImportShader.h"

#include "MayaSceneBuilder.h"
#include "Messenger.h"

#include "sharedFile/Iff.h"
#include "sharedFoundation/Tag.h"

#include "maya/MArgList.h"
#include "maya/MGlobal.h"

#include <string>
#include <vector>
#include <cstdlib>

// ======================================================================

static std::string resolveTexturePath(const std::string &treeFilePath, const std::string &inputFilename)
{
	if (treeFilePath.empty())
		return std::string();

	std::string normalizedInput = inputFilename;
	for (std::string::size_type i = 0; i < normalizedInput.size(); ++i)
	{
		if (normalizedInput[i] == '\\')
			normalizedInput[i] = '/';
	}

	std::string baseDir;

	const char *envExportRoot = getenv("TITAN_EXPORT_ROOT");
	if (envExportRoot && envExportRoot[0])
	{
		baseDir = envExportRoot;
		if (!baseDir.empty() && baseDir[baseDir.size() - 1] != '/')
			baseDir.push_back('/');
	}
	else
	{
		const char *envDataRoot = getenv("TITAN_DATA_ROOT");
		if (envDataRoot && envDataRoot[0])
		{
			baseDir = envDataRoot;
			if (!baseDir.empty() && baseDir[baseDir.size() - 1] != '/')
				baseDir.push_back('/');
		}
		else
		{
			std::string::size_type cgPos = normalizedInput.find("compiled/game/");
			if (cgPos != std::string::npos)
				baseDir = normalizedInput.substr(0, cgPos + 14);
		}
	}

	if (baseDir.empty())
		return treeFilePath;

	std::string path = treeFilePath;
	for (std::string::size_type i = 0; i < path.size(); ++i)
	{
		if (path[i] == '\\')
			path[i] = '/';
	}

	// TreeFile-relative texture paths may omit "texture/" prefix
	if (path.find("texture/") != 0 && path.find("texture\\") != 0)
		path = std::string("texture/") + path;

	// Add .dds extension if path has no extension
	if (path.find_last_of('.') == std::string::npos || path.find_last_of('.') < path.find_last_of("/\\"))
		path += ".dds";

	std::string resolved = baseDir + path;
	for (std::string::size_type i = 0; i < resolved.size(); ++i)
	{
		if (resolved[i] == '\\')
			resolved[i] = '/';
	}

	return resolved;
}

// ======================================================================

namespace
{
	Messenger *messenger;

	const Tag TAG_CSHD = TAG(C,S,H,D);
	const Tag TAG_SSHT = TAG(S,S,H,T);
	const Tag TAG_MATS = TAG(M,A,T,S);
	const Tag TAG_MATL = TAG(M,A,T,L);
	const Tag TAG_TXMS = TAG(T,X,M,S);
	const Tag TAG_TXM  = TAG3(T,X,M);
	const Tag TAG_TCSS = TAG(T,C,S,S);
	const Tag TAG_TFNS = TAG(T,F,N,S);
	const Tag TAG_ARVS = TAG(A,R,V,S);
	const Tag TAG_SRVS = TAG(S,R,V,S);
	const Tag TAG_TXTR = TAG(T,X,T,R);
	const Tag TAG_CUST = TAG(C,U,S,T);
	const Tag TAG_TX1D = TAG(T,X,1,D);
	const Tag TAG_TFAC = TAG(T,F,A,C);
	const Tag TAG_PAL  = TAG3(P,A,L);
	const Tag TAG_TAG  = TAG3(T,A,G);

	struct MaterialData
	{
		Tag   tag;
		float ambient[4];
		float diffuse[4];
		float emissive[4];
		float specular[4];
		float specularPower;
	};

	struct TextureData
	{
		Tag         tag;
		uint8       placeholder;
		uint8       wrapU;
		uint8       wrapV;
		uint8       wrapW;
		uint8       filterMip;
		uint8       filterMin;
		uint8       filterMag;
		std::string path;
	};

	struct TexCoordSetEntry
	{
		Tag   tag;
		uint8 uvIndex;
	};

	struct TextureFactorEntry
	{
		Tag    tag;
		uint32 color;
	};

	struct AlphaRefEntry
	{
		Tag   tag;
		uint8 value;
	};

	struct StencilRefEntry
	{
		Tag    tag;
		uint32 value;
	};

	struct SwappableTexture
	{
		uint32      textureTag;
		int16       firstTextureIndex;
		int16       textureCount;
		std::string variableName;
		int8        isPrivate;
		int16       defaultIndex;
	};

	struct PaletteCustomization
	{
		std::string variableName;
		int8        isPrivate;
		uint32      tfactorTag;
		std::string palettePath;
		int32       defaultIndex;
	};

	// ------------------------------------------------------------------

	void readStaticShader(
		Iff &iff,
		std::vector<MaterialData> &materials,
		std::vector<TextureData> &textures,
		std::vector<TexCoordSetEntry> &texCoordSets,
		std::vector<TextureFactorEntry> &textureFactors,
		std::vector<AlphaRefEntry> &alphaRefs,
		std::vector<StencilRefEntry> &stencilRefs,
		std::string &effectName)
	{
		iff.enterForm(TAG_SSHT);
		iff.enterForm(TAG_0000);

		while (iff.getNumberOfBlocksLeft() > 0)
		{
			if (iff.isCurrentForm())
			{
				const Tag formTag = iff.getCurrentName();

				if (formTag == TAG_MATS)
				{
					iff.enterForm(TAG_MATS);
					iff.enterForm(TAG_0000);

					while (iff.getNumberOfBlocksLeft() > 0)
					{
						const Tag chunkTag = iff.getCurrentName();

						if (chunkTag == TAG_TAG)
						{
							MaterialData mat;
							iff.enterChunk(TAG_TAG);
							mat.tag = iff.read_uint32();
							iff.exitChunk(TAG_TAG);

							iff.enterChunk(TAG_MATL);
							{
								mat.ambient[0]  = iff.read_float();
								mat.ambient[1]  = iff.read_float();
								mat.ambient[2]  = iff.read_float();
								mat.ambient[3]  = iff.read_float();
								mat.diffuse[0]  = iff.read_float();
								mat.diffuse[1]  = iff.read_float();
								mat.diffuse[2]  = iff.read_float();
								mat.diffuse[3]  = iff.read_float();
								mat.emissive[0] = iff.read_float();
								mat.emissive[1] = iff.read_float();
								mat.emissive[2] = iff.read_float();
								mat.emissive[3] = iff.read_float();
								mat.specular[0] = iff.read_float();
								mat.specular[1] = iff.read_float();
								mat.specular[2] = iff.read_float();
								mat.specular[3] = iff.read_float();
								mat.specularPower = iff.read_float();
							}
							iff.exitChunk(TAG_MATL);

							materials.push_back(mat);
						}
						else
						{
							iff.enterChunk();
							iff.exitChunk();
						}
					}

					iff.exitForm(TAG_0000);
					iff.exitForm(TAG_MATS);
				}
				else if (formTag == TAG_TXMS)
				{
					iff.enterForm(TAG_TXMS);

					while (iff.getNumberOfBlocksLeft() > 0)
					{
						if (iff.isCurrentForm() && iff.getCurrentName() == TAG_TXM)
						{
							iff.enterForm(TAG_TXM);
							iff.enterForm(TAG_0001);

							TextureData tex;

							iff.enterChunk(TAG_DATA);
							{
								tex.tag         = iff.read_uint32();
								tex.placeholder = iff.read_uint8();
								tex.wrapU       = iff.read_uint8();
								tex.wrapV       = iff.read_uint8();
								tex.wrapW       = iff.read_uint8();
								tex.filterMip   = iff.read_uint8();
								tex.filterMin   = iff.read_uint8();
								tex.filterMag   = iff.read_uint8();
							}
							iff.exitChunk(TAG_DATA);

							iff.enterChunk(TAG_NAME);
							{
								std::string texPath;
								iff.read_string(texPath);
								tex.path = texPath;
							}
							iff.exitChunk(TAG_NAME);

							textures.push_back(tex);

							iff.exitForm(TAG_0001);
							iff.exitForm(TAG_TXM);
						}
						else
						{
							if (iff.isCurrentForm())
							{
								iff.enterForm();
								iff.exitForm();
							}
							else
							{
								iff.enterChunk();
								iff.exitChunk();
							}
						}
					}

					iff.exitForm(TAG_TXMS);
				}
				else if (formTag == TAG_TCSS)
				{
					iff.enterForm(TAG_TCSS);
					iff.enterChunk(TAG_0000);
					{
						while (iff.getChunkLengthLeft(1) >= 5)
						{
							TexCoordSetEntry entry;
							entry.tag     = iff.read_uint32();
							entry.uvIndex = iff.read_uint8();
							texCoordSets.push_back(entry);
						}
					}
					iff.exitChunk(TAG_0000);
					iff.exitForm(TAG_TCSS);
				}
				else if (formTag == TAG_TFNS)
				{
					iff.enterForm(TAG_TFNS);
					iff.enterChunk(TAG_0000);
					{
						while (iff.getChunkLengthLeft(1) >= 8)
						{
							TextureFactorEntry entry;
							entry.tag   = iff.read_uint32();
							entry.color = iff.read_uint32();
							textureFactors.push_back(entry);
						}
					}
					iff.exitChunk(TAG_0000);
					iff.exitForm(TAG_TFNS);
				}
				else if (formTag == TAG_ARVS)
				{
					iff.enterForm(TAG_ARVS);
					iff.enterChunk(TAG_0000);
					{
						while (iff.getChunkLengthLeft(1) >= 5)
						{
							AlphaRefEntry entry;
							entry.tag   = iff.read_uint32();
							entry.value = iff.read_uint8();
							alphaRefs.push_back(entry);
						}
					}
					iff.exitChunk(TAG_0000);
					iff.exitForm(TAG_ARVS);
				}
				else if (formTag == TAG_SRVS)
				{
					iff.enterForm(TAG_SRVS);
					iff.enterChunk(TAG_0000);
					{
						while (iff.getChunkLengthLeft(1) >= 8)
						{
							StencilRefEntry entry;
							entry.tag   = iff.read_uint32();
							entry.value = iff.read_uint32();
							stencilRefs.push_back(entry);
						}
					}
					iff.exitChunk(TAG_0000);
					iff.exitForm(TAG_SRVS);
				}
				else
				{
					iff.enterForm();
					iff.exitForm();
				}
			}
			else
			{
				const Tag chunkTag = iff.getCurrentName();

				if (chunkTag == TAG_NAME)
				{
					iff.enterChunk(TAG_NAME);
					{
						std::string name;
						iff.read_string(name);
						effectName = name;
					}
					iff.exitChunk(TAG_NAME);
				}
				else
				{
					iff.enterChunk();
					iff.exitChunk();
				}
			}
		}

		iff.exitForm(TAG_0000);
		iff.exitForm(TAG_SSHT);
	}
}

// ======================================================================

void ImportShader::install(Messenger *newMessenger)
{
	messenger = newMessenger;
}

// ----------------------------------------------------------------------

void ImportShader::remove()
{
	messenger = 0;
}

// ----------------------------------------------------------------------

void *ImportShader::creator()
{
	return new ImportShader();
}

// ======================================================================

ImportShader::ImportShader()
{
}

// ======================================================================

MStatus ImportShader::doIt(const MArgList &args)
{
	MStatus status;

	//-- parse arguments: -i <filename>
	const unsigned argCount = args.length(&status);
	MESSENGER_REJECT_STATUS(!status, ("failed to get argument count\n"));
	MESSENGER_REJECT_STATUS(argCount < 2, ("usage: importShader -i <filename>\n"));

	std::string filename;

	for (unsigned i = 0; i < argCount; ++i)
	{
		MString arg = args.asString(i, &status);
		MESSENGER_REJECT_STATUS(!status, ("failed to get argument %u\n", i));

		if (arg == "-i" && (i + 1) < argCount)
		{
			MString val = args.asString(i + 1, &status);
			MESSENGER_REJECT_STATUS(!status, ("failed to get filename argument\n"));
			filename = val.asChar();
			++i;
		}
	}

	MESSENGER_REJECT_STATUS(filename.empty(), ("no filename specified, use -i <filename>\n"));

	filename = resolveImportPath(filename);
	MESSENGER_LOG(("ImportShader: opening [%s]\n", filename.c_str()));

	//-- open the IFF file
	Iff iff(filename.c_str());

	std::vector<MaterialData>        materials;
	std::vector<TextureData>         textures;
	std::vector<TexCoordSetEntry>    texCoordSets;
	std::vector<TextureFactorEntry>  textureFactors;
	std::vector<AlphaRefEntry>       alphaRefs;
	std::vector<StencilRefEntry>     stencilRefs;
	std::string                      effectName;
	std::vector<SwappableTexture>    swappableTextures;
	std::vector<std::string>         swappableTexturePaths;
	std::vector<PaletteCustomization> paletteCustomizations;

	const Tag topTag = iff.getCurrentName();

	if (topTag == TAG_CSHD)
	{
		//-- Customizable shader: CSHD wraps SSHT + optional TXTR + TFAC
		iff.enterForm(TAG_CSHD);
		iff.enterForm(TAG_0001);

		readStaticShader(iff, materials, textures, texCoordSets, textureFactors, alphaRefs, stencilRefs, effectName);

		//-- read TXTR form if present (swappable textures)
		if (iff.getNumberOfBlocksLeft() > 0 && iff.isCurrentForm() && iff.getCurrentName() == TAG_TXTR)
		{
			iff.enterForm(TAG_TXTR);

			// DATA chunk: texture path list
			if (iff.getNumberOfBlocksLeft() > 0 && !iff.isCurrentForm() && iff.getCurrentName() == TAG_DATA)
			{
				iff.enterChunk(TAG_DATA);
				{
					const int16 texCount = iff.read_int16();
					for (int16 ti = 0; ti < texCount; ++ti)
					{
						std::string path;
						iff.read_string(path);
						swappableTexturePaths.push_back(path);
					}
				}
				iff.exitChunk(TAG_DATA);
			}

			// CUST form: customization entries
			if (iff.getNumberOfBlocksLeft() > 0 && iff.isCurrentForm() && iff.getCurrentName() == TAG_CUST)
			{
				iff.enterForm(TAG_CUST);

				while (iff.getNumberOfBlocksLeft() > 0)
				{
					if (!iff.isCurrentForm() && iff.getCurrentName() == TAG_TX1D)
					{
						iff.enterChunk(TAG_TX1D);
						{
							SwappableTexture st;
							st.textureTag        = iff.read_uint32();
							st.firstTextureIndex = iff.read_int16();
							st.textureCount      = iff.read_int16();

							std::string varName;
							iff.read_string(varName);
							st.variableName = varName;

							st.isPrivate    = iff.read_int8();
							st.defaultIndex = iff.read_int16();
							swappableTextures.push_back(st);
						}
						iff.exitChunk(TAG_TX1D);
					}
					else
					{
						if (iff.isCurrentForm())
						{
							iff.enterForm();
							iff.exitForm();
						}
						else
						{
							iff.enterChunk();
							iff.exitChunk();
						}
					}
				}

				iff.exitForm(TAG_CUST);
			}

			iff.exitForm(TAG_TXTR);
		}

		//-- read TFAC form if present (hue/palette customizations)
		if (iff.getNumberOfBlocksLeft() > 0 && iff.isCurrentForm() && iff.getCurrentName() == TAG_TFAC)
		{
			iff.enterForm(TAG_TFAC);

			while (iff.getNumberOfBlocksLeft() > 0)
			{
				if (!iff.isCurrentForm() && iff.getCurrentName() == TAG_PAL)
				{
					iff.enterChunk(TAG_PAL);
					{
						PaletteCustomization pc;

						std::string varName;
						iff.read_string(varName);
						pc.variableName = varName;

						pc.isPrivate    = iff.read_int8();
						pc.tfactorTag   = iff.read_uint32();

						std::string palPath;
						iff.read_string(palPath);
						pc.palettePath = palPath;

						pc.defaultIndex = iff.read_int32();
						paletteCustomizations.push_back(pc);
					}
					iff.exitChunk(TAG_PAL);
				}
				else
				{
					if (iff.isCurrentForm())
					{
						iff.enterForm();
						iff.exitForm();
					}
					else
					{
						iff.enterChunk();
						iff.exitChunk();
					}
				}
			}

			iff.exitForm(TAG_TFAC);
		}

		//-- skip remaining
		while (iff.getNumberOfBlocksLeft() > 0)
		{
			if (iff.isCurrentForm())
			{
				iff.enterForm();
				iff.exitForm();
			}
			else
			{
				iff.enterChunk();
				iff.exitChunk();
			}
		}

		iff.exitForm(TAG_0001);
		iff.exitForm(TAG_CSHD);
	}
	else if (topTag == TAG_SSHT)
	{
		//-- Standalone static shader
		readStaticShader(iff, materials, textures, texCoordSets, textureFactors, alphaRefs, stencilRefs, effectName);
	}
	else
	{
		MESSENGER_LOG_ERROR(("ImportShader: unrecognized top-level form, expected SSHT or CSHD\n"));
		return MS::kFailure;
	}

	//-- log what we found
	MESSENGER_LOG(("ImportShader: effect=[%s]\n", effectName.c_str()));
	MESSENGER_LOG(("ImportShader: %d materials, %d textures, %d texCoordSets, %d textureFactors\n",
		static_cast<int>(materials.size()),
		static_cast<int>(textures.size()),
		static_cast<int>(texCoordSets.size()),
		static_cast<int>(textureFactors.size())));

	if (!swappableTextures.empty())
		MESSENGER_LOG(("ImportShader: %d swappable textures, %d texture paths\n",
			static_cast<int>(swappableTextures.size()),
			static_cast<int>(swappableTexturePaths.size())));

	if (!paletteCustomizations.empty())
		MESSENGER_LOG(("ImportShader: %d palette customizations\n",
			static_cast<int>(paletteCustomizations.size())));

	for (size_t ti = 0; ti < textures.size(); ++ti)
	{
		char tagStr[5];
		ConvertTagToString(textures[ti].tag, tagStr);
		MESSENGER_LOG(("  texture [%s]: [%s]\n", tagStr, textures[ti].path.c_str()));
	}

	//-- create a Maya material from the first MAIN texture
	std::string mainTexturePath;
	for (size_t ti = 0; ti < textures.size(); ++ti)
	{
		if (textures[ti].tag == TAG(M,A,I,N))
		{
			mainTexturePath = textures[ti].path;
			break;
		}
	}

	if (mainTexturePath.empty() && !textures.empty())
		mainTexturePath = textures[0].path;

	if (!mainTexturePath.empty())
		mainTexturePath = resolveTexturePath(mainTexturePath, filename);

	std::string shaderName = filename;
	{
		const std::string::size_type lastSlash = shaderName.find_last_of("\\/");
		if (lastSlash != std::string::npos)
			shaderName = shaderName.substr(lastSlash + 1);
		const std::string::size_type dotPos = shaderName.find_last_of('.');
		if (dotPos != std::string::npos)
			shaderName = shaderName.substr(0, dotPos);
	}

	MObject shadingGroup;
	status = MayaSceneBuilder::createMaterial(shaderName, mainTexturePath, shadingGroup);
	if (status)
		MESSENGER_LOG(("ImportShader: created material [%s] with texture [%s]\n", shaderName.c_str(), mainTexturePath.c_str()));
	else
		MESSENGER_LOG_WARNING(("ImportShader: failed to create material [%s]\n", shaderName.c_str()));

	//-- store shader metadata as Maya string attributes on the shading group
	if (!effectName.empty())
	{
		MString melCmd = "addAttr -ln \"soe_effect\" -dt \"string\" ";
		melCmd += shaderName.c_str();
		IGNORE_RETURN(MGlobal::executeCommand(melCmd));

		melCmd = "setAttr -type \"string\" ";
		melCmd += shaderName.c_str();
		melCmd += ".soe_effect \"";
		melCmd += effectName.c_str();
		melCmd += "\"";
		IGNORE_RETURN(MGlobal::executeCommand(melCmd));
	}

	MESSENGER_LOG(("ImportShader: done\n"));

	return MStatus(MStatus::kSuccess);
}

// ======================================================================
