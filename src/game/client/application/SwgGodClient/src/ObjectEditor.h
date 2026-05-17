// ======================================================================
//
// ObjectEditor.h
// copyright(c) 2001 Sony Online Entertainment
//
// ======================================================================

#ifndef INCLUDED_ObjectEditor_H
#define INCLUDED_ObjectEditor_H

// ======================================================================

#include "BaseObjectEditor.h"

#include <string>
#include <vector>

// ======================================================================
class ClientObject;
namespace MessageDispatch
{
	class Callback;
}

struct UpdateObjects;
//-----------------------------------------------------------------

/**
* The ObjectEditor allows the user to browse and modify all object properties.
*
* Browse and edit object attributes, attached scripts, and objvars.
*/
class ObjectEditor : public BaseObjectEditor, public MessageDispatch::Receiver
{
	Q_OBJECT; //lint !e1516 !e19 !e1924 !e1762 various deficiencies in the Qt macro

public:
	explicit ObjectEditor(QWidget*theParent=0, const char*theName=0);
	virtual ~ObjectEditor();

public slots:
	void refreshObjects();
	void updateObjectData();
	void onAttributeRenamed(QListViewItem* item, int col, const QString &text) const;
	void onAttributeDoubleClicked(QListViewItem* item);
	void onCreatureSkillsContextMenuRequested(QListBoxItem *, const QPoint &);
	void onRevokeCreatureSkill();
	void onRemoveScript();
	void onAttachScript();
	void onRemoveObjvar();
	void onSetObjvar();
	void onRefreshLists();
	void onScriptsListContextMenuRequested(QListViewItem *, const QPoint &, int);
	void onScriptsListDoubleClicked(QListViewItem * item);
	void onObjvarListContextMenuRequested(QListViewItem *, const QPoint &, int);
	void onObjvarRenamed(QListViewItem* item, int col, const QString &text);
	void onObjvarDoubleClicked(QListViewItem* item);
	void onObjvarRenameName();
	void onObjvarEditValue();
public:
	virtual void receiveMessage(const MessageDispatch::Emitter& source, const MessageDispatch::MessageBase& message);

protected:
	void dragEnterEvent(QDragEnterEvent* event);
	void dropEvent     (QDropEvent* event);

private:
	//disabled
	ObjectEditor(const ObjectEditor& rhs);
	ObjectEditor& operator=(const ObjectEditor& rhs);

	void updateObjects(const UpdateObjects *);

private:
	struct PropertiesMenuItems
	{
		struct Client
		{
			struct Transform
			{
				QListViewItem*   translation;
				QListViewItem*     translateX;
				QListViewItem*     translateY;
				QListViewItem*     translateZ;
				QListViewItem*   rotation;
				QListViewItem*     pitch;
				QListViewItem*     yaw;
				QListViewItem*     roll;
				QListViewItem*   scale;
				QListViewItem*     scaleX;
				QListViewItem*     scaleY;
				QListViewItem*     scaleZ;
			};

			struct General
			{
				QListViewItem*   networkId;
				QListViewItem*   authoritative;
				QListViewItem*   watchable;
				QListViewItem*   active;
				QListViewItem*   appearance;
				QListViewItem*   objectTemplate;
				QListViewItem*   volume;
				QListViewItem*   script;
				QListViewItem*   slotType;
				QListViewItem*   name;
				QListViewItem*   container;
			};

			Transform         transform;
			General           general;

			QListViewItem*   transformItem;
			QListViewItem*   generalItem;
		};

		struct Tangible
		{
			QListViewItem*   weight;
			QListViewItem*   HP;
			QListViewItem*   maxHP;
			QListViewItem*   visible;
			QListViewItem*   popupHelp;
			QListViewItem*   armorEffectiveness;
			QListViewItem*   frozen;
			QListViewItem*   squelch;
			QListViewItem*   resourceList;

			QListViewItem*   tangibleItem;
		};

		struct Creature
		{
			struct Attributes
			{
				QListViewItem*    health;
				QListViewItem*    strength;
				QListViewItem*    constitution;
				QListViewItem*    action;
				QListViewItem*    quickness;
				QListViewItem*    stamina;
				QListViewItem*    mind;
				QListViewItem*    focus;
				QListViewItem*    willpower;
			};

			Attributes         attribs;
			Attributes         maxAttribs;

			QListViewItem*    mood;
			QListViewItem*    sayMode;
			QListViewItem*    gender;
			QListViewItem*    attribsItem;
			QListViewItem*    maxAttribsItem;

			QListViewItem*    creatureItem;
		};

		Client   client;
		Tangible tangible;
		Creature creature;

		struct Trigger
		{
			stdvector<QListViewItem*>::fwd triggers;
		};

		Trigger trigger;
	};

	PropertiesMenuItems     m_pmi;
	ClientObject*           m_obj;
	MessageDispatch::Callback * m_callback;
	std::string             m_objvarEditOldName;
	void applyObjvarValue(ClientObject const & obj, std::string const & objvarName, std::string const & type, std::string const & valueText);
	void openScriptSourceInEditor(std::string const & scriptClasspath) const;
	void populateObjvarListFromLines(std::vector<std::string> const & lines);
	void editObjvarItem(QListViewItem * item);
	void addInfoClientObject();
	void addInfoTangibleObject();
	void addInfoCreatureObject();
};

// ======================================================================

#endif
