all: doubleclick.exe

doubleclick.obj: doubleclick.cpp resource.h
	cl /c /MT /O2 /GS- /GL /Gw /DWIN32 /DWIN32_LEAN_AND_MEAN /DUNICODE doubleclick.cpp

doubleclick.exe: doubleclick.obj doubleclick.res
	link /LTCG /OPT:REF /OPT:ICF doubleclick.obj doubleclick.res user32.lib kernel32.lib shell32.lib gdi32.lib comctl32.lib

clean:
	del *.obj
	del *.res
	del *.lib
	del *.exp
