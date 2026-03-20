#ifndef _ufeSceneItemFwd
#define _ufeSceneItemFwd
// ===========================================================================
// Copyright 2023 Autodesk, Inc. All rights reserved.
//
// Use of this software is subject to the terms of the Autodesk license
// agreement provided at the time of installation or download, or which
// otherwise accompanies this software in either electronic or hard copy form.
// ===========================================================================

#include "ufe.h"

#include <memory>

UFE_NS_DEF {

class SceneItem;
class UndoableCommand;

using SceneItemPtr = std::shared_ptr<SceneItem>;
using UndoableCommandPtr = std::shared_ptr<UndoableCommand>;
}

#endif  /* _ufeSceneItemFwd */
