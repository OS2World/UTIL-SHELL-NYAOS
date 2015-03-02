#include "ishell.h"
#include "nnhash.h"

Status InteractiveShell::readline( NnString &line )
{
    NnObject *val;

    if( nesting.size() > 0 ){
        dosShell.setPrompt( *(NnString*)nesting.top() );
    }else if( (val=properties.get("prompt")) != NULL ){
	dosShell.setPrompt( *(NnString*)val );
    }else{
        NnString prompt( getEnv("PROMPT") );
        dosShell.setPrompt( prompt );
    }
    return dosShell(line);
}
