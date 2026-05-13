#pragma warning (disable:4505)

#include "../../../../../../engine/shared/library/sharedFoundation/include/public/sharedFoundation/FirstSharedFoundation.h"

// Fix Windows macro conflicts with STLport template parameter names
// Windows RPC headers define 'small' as a macro (RPC data type)
#ifdef small
#undef small
#endif

// Windows SDK sal.h defines empty SAL macros __in, __out, etc. STLport uses __in as a
// parameter name in _num_get.h (do_get(__in, ...)); expanding __in breaks the argument list.
#ifdef __in
#undef __in
#endif
#ifdef __in_opt
#undef __in_opt
#endif
#ifdef __out
#undef __out
#endif
#ifdef __out_opt
#undef __out_opt
#endif
#ifdef __in_ecount
#undef __in_ecount
#endif
#ifdef __out_ecount
#undef __out_ecount
#endif
#ifdef __in_bcount
#undef __in_bcount
#endif
#ifdef __out_bcount
#undef __out_bcount
#endif

#include "sharedMessageDispatch/Emitter.h"
#include "sharedMessageDispatch/Message.h"
#include "sharedMessageDispatch/Receiver.h"
#include "StringId.h"

//commonly used STL containers
#include <string>
#include <vector>
#include <list>

//commonly used Qt classes
#include <qobject.h>
#include <qwidget.h>
#include <qcursor.h>

// Re-apply after Qt includes (Qt may re-include Windows headers)
#ifdef small
#undef small
#endif
#ifdef __in
#undef __in
#endif
#ifdef __in_opt
#undef __in_opt
#endif
#ifdef __out
#undef __out
#endif
#ifdef __out_opt
#undef __out_opt
#endif
#ifdef __in_ecount
#undef __in_ecount
#endif
#ifdef __out_ecount
#undef __out_ecount
#endif
#ifdef __in_bcount
#undef __in_bcount
#endif
#ifdef __out_bcount
#undef __out_bcount
#endif


