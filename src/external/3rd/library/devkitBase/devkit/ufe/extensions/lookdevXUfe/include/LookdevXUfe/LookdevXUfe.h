#ifndef _ufeLookdevXUfe
#define _ufeLookdevXUfe
// ===========================================================================
// Copyright 2024 Autodesk, Inc. All rights reserved.
//
// Use of this software is subject to the terms of the Autodesk license
// agreement provided at the time of installation or download, or which
// otherwise accompanies this software in either electronic or hard copy form.
// ===========================================================================

#define LOOKDEVXUFE_MAJOR_VERSION  1
#define LOOKDEVXUFE_MINOR_VERSION  0
#define LOOKDEVXUFE_PATCH_LEVEL    3

// LookdevXUfe public namespace string will never change.
#define LOOKDEVXUFE_NS LookdevXUfe
// C preprocessor trickery to expand arguments.
#define LOOKDEVXUFE_CONCAT(A, B) LOOKDEVXUFE_CONCAT_IMPL(A, B)
#define LOOKDEVXUFE_CONCAT_IMPL(A, B) A##B
// Versioned namespace includes the major version number.
#define LOOKDEVXUFE_VERSIONED_NS LOOKDEVXUFE_CONCAT(LOOKDEVXUFE_NS, _v1)
 
namespace LOOKDEVXUFE_VERSIONED_NS {}
// With a using namespace declaration, pull in the versioned namespace into the
// Ufe public namespace, to allow client code to use the plain LookdevXUfe namespace,
// e.g. LookdevXUfe::FileHandler.
namespace LOOKDEVXUFE_NS {
    using namespace LOOKDEVXUFE_VERSIONED_NS;
}

// Macros to place the LookdevXUfe symbols in the versioned namespace, which is how
// they will appear in the shared library, e.g. LookdevXUfe_v1::FileHandler.
#ifdef DOXYGEN
#define LOOKDEVXUFE_NS_DEF namespace LOOKDEVXUFE_NS
#else
#define LOOKDEVXUFE_NS_DEF namespace LOOKDEVXUFE_VERSIONED_NS
#endif 

#endif /* _ufeLookdevXUfe */
