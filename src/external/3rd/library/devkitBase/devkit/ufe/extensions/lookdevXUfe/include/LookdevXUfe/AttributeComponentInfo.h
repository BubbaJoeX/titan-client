// =======================================================================
// Copyright 2024 Autodesk, Inc. All rights reserved.
//
// This computer source code and related instructions and comments are the
// unpublished confidential  and proprietary information of Autodesk, Inc.
// and are protected under applicable copyright and trade secret law. They
// may not be disclosed to, copied  or used by any third party without the
// prior written consent of Autodesk, Inc.
// =======================================================================

#ifndef ATTRIBUTE_COMPONENT_INFO_H
#define ATTRIBUTE_COMPONENT_INFO_H

#include "Export.h"

#include <ufe/attribute.h>

LOOKDEVXUFE_NS_DEF
{

/*!
  \class AttributeComponentInfo
  \brief This class keeps attribute and attribute component information.

  This class keeps attribute and attribute component information when long-term usage is
  mandatory such as an undo/redo commands for example. The information remains valid even
  if the node owning this attribute is deleted. The Attribute class keeps a handle associated
  to a specific node instance so, deleting the node instance invalidates the Attribute
  instance.

  Fully reimplemented because the base class in UFE has explicitly deleted the copy and
  assignment semantics. Some compilers will complain if a derived class tries to expose
  such deleted functionality.
*/
class LOOKDEVX_UFE_EXPORT AttributeComponentInfo
{
public:
    //! Constuctor1
    AttributeComponentInfo(const Ufe::Attribute::Ptr& attr, const std::string& component);
    //! Constructor2
    AttributeComponentInfo(const Ufe::Path& path, const std::string& name, const std::string& component);
    //! Copy constructor
    AttributeComponentInfo(const AttributeComponentInfo&) = default;
    //! Destructor
    virtual ~AttributeComponentInfo();

    //@{
    // NOLINTBEGIN(clang-diagnostic-defaulted-function-deleted)
    AttributeComponentInfo& operator=(const AttributeComponentInfo&) = default;
    AttributeComponentInfo(AttributeComponentInfo&&) = default;
    AttributeComponentInfo& operator=(AttributeComponentInfo&&) = default;

    // NOLINTEND(clang-diagnostic-defaulted-function-deleted)
    //@}

    //! Returns the attribute component. Empty string if referencing the attribute and not a component.
    inline std::string component() const
    {
        return m_component;
    }

    // From Ufe::AttributeInfo:
    inline Ufe::Rtid runTimeId() const
    {
        return m_nodePath.runTimeId();
    }

    inline Ufe::Path path() const
    {
        return m_nodePath;
    }

    inline std::string name() const
    {
        return m_attributeName;
    }

    /*!
     * \brief Create the Attribute from the AttributeInfo.
     * \note This method could have a performance hit.
     */
    Ufe::Attribute::Ptr attribute() const;

    bool operator==(const AttributeComponentInfo& other)
    {
        return other.m_attributeName == m_attributeName && other.m_component == m_component &&
               other.m_nodePath == m_nodePath;
    }

private:
    Ufe::Path m_nodePath;
    std::string m_attributeName;
    std::string m_component;
};

} // LOOKDEVXUFE_NS_DEF

#endif // ATTRIBUTE_COMPONENT_INFO_H
