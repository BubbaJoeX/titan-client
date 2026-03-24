TEMPLATE = app
LANGUAGE = C++

CONFIG += qt warn_on release

SOURCES += ../../src/main.cpp

win32:LIBS += user32.lib
