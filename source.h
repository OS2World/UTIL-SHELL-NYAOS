#ifndef SOURCE_H
#define SOURCE_H

#include "nnstring.h"

class NnVector;
class NyadosShell;

extern NnString rcfname;
extern void callrc( const NnString &rcfname_ , const NnVector &argv );
extern void seek_and_call_rc( const char *argv0 , const NnVector &argv );
extern int  cmd_source( NyadosShell &shell , const NnString &argv );

#endif
