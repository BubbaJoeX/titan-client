cmake_minimum_required(VERSION 3.13)

project(unicode)

add_definitions(/D_CRT_SECURE_NO_WARNINGS)

set(SHARED_SOURCES
        FirstUnicode.h
        UnicodeBlocks.cpp
        UnicodeBlocks.h
        UnicodeCharacterData.cpp
        UnicodeCharacterData.h
        UnicodeCharacterDataMap.cpp
        UnicodeCharacterDataMap.h
        Unicode.cpp
        Unicode.h
        UnicodeUtils.cpp
        UnicodeUtils.h
        utf8.cpp
        )

add_library(unicode
        ${SHARED_SOURCES}
        )