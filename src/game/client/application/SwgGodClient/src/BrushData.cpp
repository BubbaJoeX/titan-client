// ======================================================================
//
// BrushData.cpp
// copyright (c) 2001 Sony Online Entertainment
//
// ======================================================================

#include "SwgGodClient/FirstSwgGodClient.h"
#include "BrushData.h"

#include "sharedFile/Iff.h"
#include "sharedFoundation/PointerDeleter.h"

#include "clientGame/Game.h"

#include "FilesystemTree.h"

#include <algorithm>
#include <stdio.h>
#include <direct.h>

// ======================================================================

const char* const  BrushData::Messages::BRUSHES_CHANGED = "BrushData::Messages::BRUSHES_CHANGED";
const char* const  BrushData::Messages::BRUSH_DRAGGED   = "BrushData::Messages::BRUSH_DRAGGED";

namespace BrushDataNamespace
{
	const std::string cs_singlePlayerOnly = " (Single Player Only)";
}

using namespace BrushDataNamespace;

//-----------------------------------------------------------------

struct BrushData::BrushContainer
{
	typedef std::vector<BrushStruct> Container;
	typedef BrushData::BrushContainer::Container::iterator Iterator;
	BrushData::BrushContainer::Container  container;
};

//-----------------------------------------------------------------

BrushData::BrushData()
: Singleton <BrushData>(),
  MessageDispatch::Emitter(),
  m_brushes(0),
  m_selectedBrush("")
{
	m_brushes = new BrushContainer;
}

//----------------------------------------------------------------------

BrushData::~BrushData()
{
	//go through each brush and clean it out
	for(BrushContainer::Container::iterator it = m_brushes->container.begin(); it != m_brushes->container.end(); ++it)
	{
		std::for_each(it->objects.begin(), it->objects.end(), PointerDeleter());
	}
	delete m_brushes;
	m_brushes = 0;
}

//-----------------------------------------------------------------

void BrushData::addBrush(const std::string & name, const GodClientData::ClipboardList_t& objects, real pasteGroundAnchorY, bool pasteGroundAnchorKnown)
{
	BrushStruct newBrush;
	newBrush.name = name;
	newBrush.objects = objects;
	newBrush.pasteGroundAnchorY = pasteGroundAnchorY;
	newBrush.pasteGroundAnchorKnown = pasteGroundAnchorKnown;

	m_brushes->container.push_back(newBrush);

	MessageDispatch::MessageBase m(Messages::BRUSHES_CHANGED);
	emitMessage(m);
	serialize();
}

//-----------------------------------------------------------------

void BrushData::removeBrush(size_t index)
{
	DEBUG_FATAL(index > m_brushes->container.size (), ("out of range\n"));
	
	//TODO move this to the app start, since the current working dir may change during use
	char        applicationDirectory [2048];
	IGNORE_RETURN(_getcwd(applicationDirectory, 2048));

	char dirBuffer[2048];
	sprintf(dirBuffer, "%s\\%s", applicationDirectory, "GodClientData");

	char nameBuffer[2048];
	sprintf (nameBuffer, "%s\\%s.%s", dirBuffer, m_brushes->container[index].name.c_str(), "bru");
	//remove the brush file from disk
	int result = _unlink(nameBuffer);
	UNREF(result);
	//remove the file from the data structre
	IGNORE_RETURN(m_brushes->container.erase (m_brushes->container.begin () + index));
	MessageDispatch::MessageBase m(Messages::BRUSHES_CHANGED);
	//let anyone know cares know about the change (i.e. the GUI)
	emitMessage(m);
	serialize();
}

//-----------------------------------------------------------------

size_t BrushData::getNumBrushes() const
{
	return m_brushes->container.size();
}

//-----------------------------------------------------------------

const BrushData::BrushStruct& BrushData::getBrush(size_t index) const
{
	DEBUG_FATAL (index > m_brushes->container.size (), ("out of range\n"));
	return m_brushes->container [index];
}

//-----------------------------------------------------------------

void BrushData::serialize() const
{
	bool brushIsForSinglePlayer = false;

	//for each brush, write it out
	for(BrushContainer::Iterator it = m_brushes->container.begin(); it != m_brushes->container.end(); ++it)
	{
		const BrushStruct & brush = (*it);

		brushIsForSinglePlayer = false;
		std::string name = brush.name;

		GodClientData::ClipboardList_t::const_iterator brushObjectsIterator = brush.objects.begin();
		GodClientData::ClipboardObject* const obj = NON_NULL(*brushObjectsIterator);
		if(obj && obj->serverObjectTemplateName.empty())
		{
			brushIsForSinglePlayer = true;
			if(name.find(cs_singlePlayerOnly) == name.npos)
				name += cs_singlePlayerOnly;
		}

		Iff iff(10 * 1024);
		const Tag TAG_BRSH = TAG(B,R,S,H);
		const Tag TAG_DATA = TAG(D,A,T,A);
		const Tag TAG_CLIP = TAG(C,L,I,P);
		const Tag TAG_GNDY = TAG(G,N,D,Y);
		iff.insertForm(TAG_BRSH);
		{
			// 0004 = 0003 plus paste ground Y (terrain height at brush center for relative vertical paste).
			iff.insertForm(TAG_0004);
			{
				iff.insertChunk(TAG_DATA);
					iff.insertChunkString(name.c_str());
				iff.exitChunk(TAG_DATA);

				real const anchorForFile = brush.pasteGroundAnchorKnown ? brush.pasteGroundAnchorY : GodClientData::getInstance().computePasteGroundAnchorY(brush.objects);
				iff.insertChunk(TAG_GNDY);
					iff.insertChunkData(anchorForFile);
				iff.exitChunk(TAG_GNDY);

				for(GodClientData::ClipboardList_t::const_iterator itr = brush.objects.begin(); itr != brush.objects.end(); ++itr)
				{
					iff.insertChunk(TAG_CLIP);
						GodClientData::ClipboardObject* const clipboardObject = NON_NULL(*itr);

						DEBUG_FATAL (!brushIsForSinglePlayer && clipboardObject->serverObjectTemplateName.empty (), ("Bad brush data, can't serialize"));
						iff.insertChunkString (clipboardObject->serverObjectTemplateName.c_str ());
						iff.insertChunkString (clipboardObject->sharedObjectTemplateName.c_str ());
						iff.insertChunkFloatTransform (clipboardObject->transform);
						iff.insertChunkString (clipboardObject->networkId.getValueString ().c_str ());
						iff.insertChunkFloatVector (clipboardObject->scale);
						int32 const scriptCount = static_cast<int32>(clipboardObject->scripts.size ());
						iff.insertChunkData (scriptCount);
						for (size_t si = 0; si < clipboardObject->scripts.size (); ++si)
							iff.insertChunkString (clipboardObject->scripts[si].c_str ());
						int32 const objvarCount = static_cast<int32>(clipboardObject->objvars.size ());
						iff.insertChunkData (objvarCount);
						for (size_t oi = 0; oi < clipboardObject->objvars.size (); ++oi)
							iff.insertChunkString (clipboardObject->objvars[oi].c_str ());
					iff.exitChunk(TAG_CLIP);
				}
			}
			iff.exitForm(TAG_0004);
		}
		iff.exitForm(TAG_BRSH);

		// write out the iff
		char        applicationDirectory [2048];
		IGNORE_RETURN(_getcwd(applicationDirectory, 2048));

		char writeDirBuffer[2048];
		sprintf(writeDirBuffer, "%s\\%s", applicationDirectory, "GodClientData");
		IGNORE_RETURN(_mkdir(writeDirBuffer));

		char nameBuffer[2048];
		sprintf (nameBuffer, "%s\\%s.%s", writeDirBuffer, brush.name.c_str(), "bru");

		bool result = iff.write(nameBuffer, true);
		UNREF(result);
	}
}

//-----------------------------------------------------------------

void BrushData::unserialize()
{
	//clear out what's currently a brush, load the new ones from disk
	m_brushes->container.clear();

	//TODO move this to the app start, since the current working dir may change during use
	char applicationDirectory [2048];
	IGNORE_RETURN(_getcwd(applicationDirectory, 2048));

	char readDirBuffer[2048];
	sprintf(readDirBuffer, "%s\\%s", applicationDirectory, "GodClientData");

	const Tag TAG_CLIP = TAG(C,L,I,P);
	const Tag TAG_BRSH = TAG(B,R,S,H);
	const Tag TAG_DATA = TAG(D,A,T,A);
	const Tag TAG_GNDY = TAG(G,N,D,Y);

	FilesystemTree tree;
	tree.setRootPath(readDirBuffer);
	tree.populateTree();

	const AbstractFilesystemTree::Node* rootNode = tree.getRootNode();

	//for each file, read it in if it's a brush
	for(AbstractFilesystemTree::Node::ConstIterator it = rootNode->begin(); it != rootNode->end(); ++it)
	{
		//skip directories
		if((*it)->type == AbstractFilesystemTree::Node::Folder)
			continue;
		else if((*it)->type == AbstractFilesystemTree::Node::File)
		{
			//if it's a brush, read it in
			if((*it)->name.find(".bru") != std::string::npos) //lint !e737 implicit promotion, bug in STL with size_t and std::string::npos being of different signage
			{
				char nameBuffer[2048];
				sprintf (nameBuffer, "%s\\%s", readDirBuffer, (*it)->name.c_str());

				std::string dummy;

				Iff iff;
				if (iff.open (nameBuffer, true))
				{
					BrushStruct newBrush;

					iff.enterForm (TAG_BRSH);
					{
						switch (iff.getCurrentName ())
						{
						case TAG_0000:
							{
								iff.enterForm (TAG_0000);
								{
									newBrush.pasteGroundAnchorY = 0;
									newBrush.pasteGroundAnchorKnown = false;
									iff.enterChunk (TAG_DATA);
										iff.read_string (newBrush.name);
									iff.exitChunk (TAG_DATA, true);

									while (iff.getNumberOfBlocksLeft ())
									{
										iff.enterChunk (TAG_CLIP);

											GodClientData::ClipboardObject* const clipboardObject = new GodClientData::ClipboardObject;
											iff.read_string (dummy);
											iff.read_string (clipboardObject->serverObjectTemplateName);
											iff.read_string (dummy);
											clipboardObject->transform = iff.read_floatTransform ();
											clipboardObject->networkId = NetworkId (static_cast<NetworkId::NetworkIdType> (iff.read_int32 ()));
											IGNORE_RETURN (iff.read_int32 ());

										iff.exitChunk (TAG_CLIP);

										newBrush.objects.push_back (clipboardObject);
									}
								}
								iff.exitForm (TAG_0000);
							}
							break;

						case TAG_0001:
							{
								iff.enterForm (TAG_0001);
								{
									newBrush.pasteGroundAnchorY = 0;
									newBrush.pasteGroundAnchorKnown = false;
									iff.enterChunk (TAG_DATA);
										iff.read_string (newBrush.name);
									iff.exitChunk (TAG_DATA, true);

									while (iff.getNumberOfBlocksLeft ())
									{
										iff.enterChunk (TAG_CLIP);

											GodClientData::ClipboardObject* const clipboardObject = new GodClientData::ClipboardObject;
											iff.read_string (clipboardObject->serverObjectTemplateName);
											iff.read_string (dummy);
											clipboardObject->transform = iff.read_floatTransform ();
											clipboardObject->networkId = NetworkId (static_cast<NetworkId::NetworkIdType> (iff.read_int32 ()));
											IGNORE_RETURN (iff.read_int32 ());

										iff.exitChunk (TAG_CLIP);

										newBrush.objects.push_back (clipboardObject);
									}
								}
								iff.exitForm (TAG_0001);
							}
							break;

						case TAG_0002:
							{
								std::string networkId;

								iff.enterForm (TAG_0002);
								{
									newBrush.pasteGroundAnchorY = 0;
									newBrush.pasteGroundAnchorKnown = false;
									iff.enterChunk (TAG_DATA);
										iff.read_string (newBrush.name);
									iff.exitChunk (TAG_DATA, true);

									while (iff.getNumberOfBlocksLeft ())
									{
										iff.enterChunk (TAG_CLIP);

											GodClientData::ClipboardObject* const clipboardObject = new GodClientData::ClipboardObject;
											iff.read_string (clipboardObject->serverObjectTemplateName);
											iff.read_string (clipboardObject->sharedObjectTemplateName);
											clipboardObject->transform = iff.read_floatTransform ();
											iff.read_string (networkId);
											clipboardObject->networkId = NetworkId (networkId);

										iff.exitChunk (TAG_CLIP);

										newBrush.objects.push_back (clipboardObject);
									}
								}
								iff.exitForm (TAG_0002);
							}
							break;

						case TAG_0003:
							{
								std::string networkId;

								iff.enterForm (TAG_0003);
								{
									newBrush.pasteGroundAnchorY = 0;
									newBrush.pasteGroundAnchorKnown = false;
									iff.enterChunk (TAG_DATA);
										iff.read_string (newBrush.name);
									iff.exitChunk (TAG_DATA, true);

									while (iff.getNumberOfBlocksLeft ())
									{
										iff.enterChunk (TAG_CLIP);

											GodClientData::ClipboardObject* const clipboardObject = new GodClientData::ClipboardObject;
											iff.read_string (clipboardObject->serverObjectTemplateName);
											iff.read_string (clipboardObject->sharedObjectTemplateName);
											clipboardObject->transform = iff.read_floatTransform ();
											iff.read_string (networkId);
											clipboardObject->networkId = NetworkId (networkId);
											clipboardObject->scale = iff.read_floatVector ();
											int32 const scriptCount = iff.read_int32 ();
											for (int si = 0; si < scriptCount; ++si)
											{
												std::string line;
												iff.read_string (line);
												clipboardObject->scripts.push_back (line);
											}
											int32 const objvarCount = iff.read_int32 ();
											for (int oi = 0; oi < objvarCount; ++oi)
											{
												std::string line;
												iff.read_string (line);
												clipboardObject->objvars.push_back (line);
											}

										iff.exitChunk (TAG_CLIP);

										newBrush.objects.push_back (clipboardObject);
									}
								}
								iff.exitForm (TAG_0003);
							}
							break;

						case TAG_0004:
							{
								std::string networkId;

								iff.enterForm (TAG_0004);
								{
									newBrush.pasteGroundAnchorY = 0;
									newBrush.pasteGroundAnchorKnown = false;
									iff.enterChunk (TAG_DATA);
										iff.read_string (newBrush.name);
									iff.exitChunk (TAG_DATA, true);

									iff.enterChunk (TAG_GNDY);
										newBrush.pasteGroundAnchorY = iff.read_float ();
										newBrush.pasteGroundAnchorKnown = true;
									iff.exitChunk (TAG_GNDY, true);

									while (iff.getNumberOfBlocksLeft ())
									{
										iff.enterChunk (TAG_CLIP);

											GodClientData::ClipboardObject* const clipboardObject = new GodClientData::ClipboardObject;
											iff.read_string (clipboardObject->serverObjectTemplateName);
											iff.read_string (clipboardObject->sharedObjectTemplateName);
											clipboardObject->transform = iff.read_floatTransform ();
											iff.read_string (networkId);
											clipboardObject->networkId = NetworkId (networkId);
											clipboardObject->scale = iff.read_floatVector ();
											int32 const scriptCount = iff.read_int32 ();
											for (int si = 0; si < scriptCount; ++si)
											{
												std::string line;
												iff.read_string (line);
												clipboardObject->scripts.push_back (line);
											}
											int32 const objvarCount = iff.read_int32 ();
											for (int oi = 0; oi < objvarCount; ++oi)
											{
												std::string line;
												iff.read_string (line);
												clipboardObject->objvars.push_back (line);
											}

										iff.exitChunk (TAG_CLIP);

										newBrush.objects.push_back (clipboardObject);
									}
								}
								iff.exitForm (TAG_0004);
							}
							break;

						default:
							iff.enterForm ();
							iff.exitForm (true);
							break;
						}
					}
					iff.exitForm (TAG_BRSH);

					m_brushes->container.push_back (newBrush);
				}
			}
		}
	}
	//let anyone know cares know about the change (i.e. the GUI)
	MessageDispatch::MessageBase m(Messages::BRUSHES_CHANGED);
	emitMessage(m);
}

//-----------------------------------------------------------------

void BrushData::setSelectedBrush(const std::string& name)
{
	m_selectedBrush = name;
}

//-----------------------------------------------------------------

const GodClientData::ClipboardList_t* BrushData::getSelectedBrush() const
{
	if(m_selectedBrush.empty())
		return NULL;

	for(BrushContainer::Container::iterator it = m_brushes->container.begin(); it != m_brushes->container.end(); ++it)
	{
		if(it->name == m_selectedBrush)
			return &(it->objects);
	}
	return NULL;
}

//-----------------------------------------------------------------

const BrushData::BrushStruct* BrushData::getSelectedBrushStruct() const
{
	if(m_selectedBrush.empty())
		return NULL;

	for(BrushContainer::Container::const_iterator it = m_brushes->container.begin(); it != m_brushes->container.end(); ++it)
	{
		if(it->name == m_selectedBrush)
			return &(*it);
	}
	return NULL;
}

// ======================================================================


/*
const GodClientData::ClipboardList_t* BrushData::getSelectedBrush() const
{
	if(m_selectedBrush.empty())
		return NULL;

	for(BrushContainer::Container::iterator it = m_brushes->container.begin(); it != m_brushes->container.end(); ++it)
	{
		if(it->name == m_selectedBrush)
		{
			GodClientData::ClipboardList_t* newCB = new GodClientData::ClipboardList_t;
			std::copy(it->objects.begin(), it->objects.end(), std::back_inserter(*newCB));
			return newCB;
		}
	}
	return NULL;
}
*/