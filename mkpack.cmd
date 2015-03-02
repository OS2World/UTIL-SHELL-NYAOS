@echo off
if not '%1' == '' goto main
    echo Usage: %0 VERSION-STRING
    goto exit
:main
if not '%OS%' == 'Windows_NT' goto OS2
    zip -r nyaos-%1-win.zip nyaos.exe nyaos*.txt _nya _nya.d\* -x _nya.d\.hg*
    hg archive -t zip nyaos-%1-src.zip
    goto exit
:OS2
    zip -r nyaos-%1-os2.zip nyaos.exe nyaos*.txt _nya _nya.d\* -x _nya.d\.hg*
:exit
