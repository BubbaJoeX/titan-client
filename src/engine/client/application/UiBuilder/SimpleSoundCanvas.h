#ifndef __SimpleSOUNDCANVAS_H__
#define __SimpleSOUNDCANVAS_H__

#include "UIString.h"
#include "UISoundCanvas.h"

#ifdef _WIN64
#include <unordered_map>
#else
#include <hash_map>
#endif

class SimpleSoundCanvas : public UISoundCanvas
{
public:

	typedef std::hash_map<UINarrowString, char *>  SoundMap_t;

	                 SimpleSoundCanvas();
	virtual         ~SimpleSoundCanvas( void );

	virtual void     Play( const char * );

private:

	SoundMap_t       m_soundMap;
};

#endif // __SimpleSOUNDCANVAS_H__


