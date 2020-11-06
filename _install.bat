set gamepath="c:\Program Files\Quake III Arena"

set sourcepath=c:\ioq3-pp\build\release-mingw32-x86
set appname=ioq3-pp.x86.exe

cd\
copy %sourcepath%\*.exe %gamepath%
copy %sourcepath%\*.dll %gamepath%

pause
%gamepath%\%appname% +set g_gametype 0 +devmap pro-q3dm6 +set developer 1