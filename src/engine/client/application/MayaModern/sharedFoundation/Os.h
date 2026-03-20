// ======================================================================
//
// Os.h
//
// Portions copyright 1998 Bootprint Entertainment
// Portions copyright 2000-2002 Sony Online Entertainment
// All Rights Reserved
//
// ======================================================================

#ifndef INCLUDED_Os_H
#define INCLUDED_Os_H

// ======================================================================

class Os
{
public:

    enum Priority
    {
        P_high,
        P_normal,
        P_low
    };

    enum
    {
        MAX_PATH_LENGTH = 512
    };

public:

    static bool                createDirectories (const char *dirname);

    static bool                writeFile(const char *fileName, const void *data, int length);

};

// ======================================================================

#endif
