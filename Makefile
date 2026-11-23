# ============================================================
# DMC1 PC Port - Makefile for MSVC (nmake)
#
#   nmake          собрать DMC1.exe
#   nmake run      собрать и запустить
#   nmake clean    удалить объекты и exe
#
# Put paths to your Visual Studio Build Tools and Windows SDK below.
# ============================================================

MSVC   = C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Tools\MSVC\14.52.37025
SDK    = C:\Program Files (x86)\Windows Kits\10
SDKVER = 10.0.26100.0

CC     = "$(MSVC)\bin\Hostx64\x64\cl.exe"
LINK32 = "$(MSVC)\bin\Hostx64\x64\link.exe"

INCLUDES = /Iinclude \
           /I"$(MSVC)\include" \
           /I"$(SDK)\Include\$(SDKVER)\ucrt" \
           /I"$(SDK)\Include\$(SDKVER)\um" \
           /I"$(SDK)\Include\$(SDKVER)\shared"

LIBPATHS = /LIBPATH:"$(MSVC)\lib\x64" \
           /LIBPATH:"$(SDK)\Lib\$(SDKVER)\ucrt\x64" \
           /LIBPATH:"$(SDK)\Lib\$(SDKVER)\um\x64"

LIBS = OpenGL32.lib GLU32.lib WinMM.lib Kernel32.lib User32.lib Gdi32.lib Shell32.lib

CFLAGS  = /nologo /W3 /O2 /std:c17 /DWIN32 /D_WINDOWS /DWIN32_LEAN_AND_MEAN /D_CRT_SECURE_NO_WARNINGS
LDFLAGS = /nologo /SUBSYSTEM:WINDOWS

OUT    = DMC1.exe
OBJDIR = build

OBJS = $(OBJDIR)\main.obj \
       $(OBJDIR)\win32.obj \
       $(OBJDIR)\gamepad.obj \
       $(OBJDIR)\opengl.obj \
       $(OBJDIR)\openal.obj \
       $(OBJDIR)\tim2.obj \
       $(OBJDIR)\t32.obj

HEADERS = include\sys\win32.h include\input\gamepad.h include\render\opengl.h include\audio\openal.h include\loader\tim2.h include\loader\t32.h

all : $(OUT)

$(OUT) : $(OBJS)
	$(LINK32) $(LDFLAGS) /OUT:$(OUT) $(OBJS) $(LIBS) $(LIBPATHS)

$(OBJDIR)\main.obj : src\main.c $(HEADERS)
	@if not exist $(OBJDIR) mkdir $(OBJDIR)
	$(CC) $(CFLAGS) $(INCLUDES) /c src\main.c /Fo$(OBJDIR)\main.obj

$(OBJDIR)\win32.obj : src\sys\win32.c include\sys\win32.h
	@if not exist $(OBJDIR) mkdir $(OBJDIR)
	$(CC) $(CFLAGS) $(INCLUDES) /c src\sys\win32.c /Fo$(OBJDIR)\win32.obj

$(OBJDIR)\gamepad.obj : src\input\gamepad.c include\input\gamepad.h include\sys\win32.h
	@if not exist $(OBJDIR) mkdir $(OBJDIR)
	$(CC) $(CFLAGS) $(INCLUDES) /c src\input\gamepad.c /Fo$(OBJDIR)\gamepad.obj

$(OBJDIR)\opengl.obj : src\render\opengl.c include\render\opengl.h include\sys\win32.h
	@if not exist $(OBJDIR) mkdir $(OBJDIR)
	$(CC) $(CFLAGS) $(INCLUDES) /c src\render\opengl.c /Fo$(OBJDIR)\opengl.obj

$(OBJDIR)\openal.obj : src\audio\openal.c include\audio\openal.h
	@if not exist $(OBJDIR) mkdir $(OBJDIR)
	$(CC) $(CFLAGS) $(INCLUDES) /c src\audio\openal.c /Fo$(OBJDIR)\openal.obj

$(OBJDIR)\tim2.obj : src\loader\tim2.c include\loader\tim2.h
	@if not exist $(OBJDIR) mkdir $(OBJDIR)
	$(CC) $(CFLAGS) $(INCLUDES) /c src\loader\tim2.c /Fo$(OBJDIR)\tim2.obj

$(OBJDIR)\t32.obj : src\loader\t32.c include\loader\t32.h
	@if not exist $(OBJDIR) mkdir $(OBJDIR)
	$(CC) $(CFLAGS) $(INCLUDES) /c src\loader\t32.c /Fo$(OBJDIR)\t32.obj

run : $(OUT)
	$(OUT)

# ---- standalone inspection tools (not part of the game binary) ----

dump : $(OBJDIR)\dmc_dump.exe

viewer : $(OBJDIR)\dmc_viewer.exe

tools : $(OBJDIR)\dmc_dump.exe $(OBJDIR)\dmc_viewer.exe $(OBJDIR)\t32_check.exe

$(OBJDIR)\dmc_dump.exe : tools\dmc_dump.c
	@if not exist $(OBJDIR) mkdir $(OBJDIR)
	$(CC) $(CFLAGS) $(INCLUDES) tools\dmc_dump.c /Fe$(OBJDIR)\dmc_dump.exe /Fo$(OBJDIR)\

$(OBJDIR)\dmc_viewer.exe : tools\dmc_viewer.c src\loader\tim2.c src\loader\t32.c include\loader\tim2.h include\loader\t32.h
	@if not exist $(OBJDIR) mkdir $(OBJDIR)
	$(CC) $(CFLAGS) $(INCLUDES) src\loader\tim2.c src\loader\t32.c tools\dmc_viewer.c /Fe$(OBJDIR)\dmc_viewer.exe /Fo$(OBJDIR)\ $(LIBS)

$(OBJDIR)\t32_check.exe : tools\t32_check.c src\loader\t32.c include\loader\t32.h
	@if not exist $(OBJDIR) mkdir $(OBJDIR)
	$(CC) $(CFLAGS) $(INCLUDES) src\loader\t32.c tools\t32_check.c /Fe$(OBJDIR)\t32_check.exe /Fo$(OBJDIR)\

clean :
	@if exist $(OBJDIR) rmdir /s /q $(OBJDIR)
	@if exist $(OUT) del /q $(OUT)
