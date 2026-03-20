#ifndef _ufeValue
#define _ufeValue

// =======================================================================
// Copyright 2021 Autodesk, Inc. All rights reserved.
//
// This computer source code and related instructions and comments are the
// unpublished confidential  and proprietary information of Autodesk, Inc.
// and are protected under applicable copyright and trade secret law. They 
// may not be disclosed to, copied  or used by any third party without the 
// prior written consent of Autodesk, Inc.
// =======================================================================

#include "common/ufeExport.h"

#include <memory>
#include <string>
#include <unordered_map>

UFE_NS_DEF {

//! \brief Value class that can hold a wide set of types.
/*!

This class is instantiated for the following types:
  \li \c bool
  \li \c int
  \li \c float
  \li \c double
  \li \c string
  \li \c Ufe::Vector2i
  \li \c Ufe::Vector2f
  \li \c Ufe::Vector2d
  \li \c Ufe::Vector3i
  \li \c Ufe::Vector3f
  \li \c Ufe::Vector3d
  \li \c Ufe::Vector4i
  \li \c Ufe::Vector4f
  \li \c Ufe::Vector4d
  \li \c Ufe::Color3f
  \li \c Ufe::Color4f
*/

#define UFE_VALUE_SUPPORTS_VECTOR_AND_COLOR 1

class UFE_SDK_DECL Value {
public:
    //! Default constructor. Creates an empty Value.
    Value();

    /*!
        Create a Value from the argument.
        \param v Value.
    */
    template<typename T> Value(const T& v);

    //! Default copy constructor.
    Value(const Value&);

    //! Move constructor.
    Value(Value&&) noexcept;

    //! Default assignment operator.
    Value& operator=(const Value& v);

    //! Move assignment. Right hand side becomes empty.
    Value& operator=(Value&&) noexcept;

    //! Destructor
    ~Value();

    //! \return The current value if template type matches actual type.
    //! \exception InvalidValueGet If the template type doesn't match the type of the actual value.
    template<typename T> T get() const;

    //! Returns a copy of the value if the template type matches actual type.
    //! Returns the input argument value otherwise.
    //! Especially useful to return a default when dealing with an empty Ufe::Value.
    //! \return The current value if template type matches actual type, otherwise arg.
    template<typename T> T safeGet(T arg) const;

    //! \return True iff this value is empty.
    bool empty() const;

    //! Returns the C++ type name of the contained value if non-empty.
    //! For basic types uses the typeid(T) function.
    //! For complex types will return more human readable string for
    //! easier comparison.
    //! \return Empty string if Value is empty.
    //! \return "bool", "int", "float", "double" for basic types.
    //! \return "std::string" for std::string type.
    //! \return "Ufe::Vector2i" for Ufe::Vector2i type.
    //! \return "Ufe::Vector2f" for Ufe::Vector2f type.
    //! \return "Ufe::Vector2d" for Ufe::Vector2d type.
    //! \return "Ufe::Vector3i" for Ufe::Vector3i type.
    //! \return "Ufe::Vector3f" for Ufe::Vector3f type.
    //! \return "Ufe::Vector3d" for Ufe::Vector3d type.
    //! \return "Ufe::Vector4i" for Ufe::Vector4i type.
    //! \return "Ufe::Vector4f" for Ufe::Vector4f type.
    //! \return "Ufe::Vector4d" for Ufe::Vector4d type.
    //! \return "Ufe::Color3f" for Ufe::Color3f type.
    //! \return "Ufe::Color4f" for Ufe::Color4f type.
    std::string typeName() const;

    //! \return True if the contained value is an object of type T.
    template<typename T> bool isType() const;

    //@{
    //! Equality operators.
    bool operator==(const Value& rhs) const;
    bool operator!=(const Value& rhs) const;
    //@}

private:

    struct Imp;

    std::unique_ptr<Imp> _imp;
};

using ValueDictionary = std::unordered_map<std::string, Value>;

}

#endif /* _ufeValue */
