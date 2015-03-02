local VERSION=false

for line in io.lines("version.h") do
    local m=string.match(line,"^#define%s+VER%s+\"([^\"]+)\"")
    if m then
        VERSION = m
    end
end
if not VERSION then
    print("Can not find version-text.")
    os.exit(1)
end
print("update nyacus.rc for "..VERSION )

local VERSION_COMMA = string.gsub(VERSION,"[%.%_]",",")

local fp=io.open( "nyacus.rc" , "w" )
fp:write( string.format(
[[#include <winver.h>

LUACAT ICON luacat.ico
VS_VERSION_INFO    VERSIONINFO 
FILEVERSION        %s
PRODUCTVERSION     %s
FILEFLAGSMASK      VS_FFI_FILEFLAGSMASK 
FILEFLAGS          0x00000000L
FILEOS             VOS__WINDOWS32
FILETYPE           VFT_APP
FILESUBTYPE        VFT2_UNKNOWN
BEGIN
    BLOCK "VarFileInfo"
    BEGIN
        VALUE "Translation", 0x0411, 0x04E4
    END
    BLOCK "StringFileInfo"
    BEGIN
        BLOCK "041104E4"
        BEGIN
            VALUE "CompanyName",      "NYAOS.ORG"
            VALUE "FileDescription",  "Extended Commandline Shell"
            VALUE "FileVersion",      "%s\0"
            VALUE "LegalCopyright",   "Copyright (C) 2001-2013 HAYAMA,Kaoru\0"
            VALUE "OriginalFilename", "NYAOS.EXE\0"
            VALUE "ProductName",      "Nihongo Yet Another Open Shell\0"
            VALUE "ProductVersion",   "%s\0"
        END 
    END
END
]] , VERSION_COMMA , VERSION_COMMA , VERSION , VERSION ) )
fp:close()
os.exit(0)
