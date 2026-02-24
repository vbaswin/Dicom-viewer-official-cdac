TEMPLATE = subdirs
SUBDIRS = MainApp SandBox
win32-msvc*: QMAKE_CXXFLAGS += /MP
