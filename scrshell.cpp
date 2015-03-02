#include "shell.h"

static const NnString *argv_or_null( const NnVector *argv , int n )
{
    static NnString empty;

    if( argv != NULL && n < argv->size() ){
        return (const NnString*)argv->const_at(n);
    }else{
        return &empty;
    }
}

const NnString *ScriptShell::argv(int n) const
{ return argv_or_null( &argv_ , n ); }
const NnString *BufferedShell::argv(int n) const
{ return argv_or_null( params , n ); }

void ScriptShell::shift()
{   delete argv_.shift(); }

int ScriptShell::setLabel( NnString &label )
{
    LabelOnScript *los=new LabelOnScript;
    fr->getpos( los->pos );
    labels.put( label , los );
    return 0;
}
int ScriptShell::goLabel( NnString &label )
{
    LabelOnScript *los=(LabelOnScript*)labels.get(label);
    if( los != NULL ){
        fr->setpos( los->pos );
        return 0;
    }else{
        NnString cmdline;
        fpos_t start;
        fr->getpos( start );
        for(;;){
            Status rc=this->readcommand(cmdline);
            if( rc == TERMINATE ){
                fr->setpos( start );
                return 0;
            }
            cmdline.trim();
            if( cmdline.at(0) == ':' ){
                NnString label2( cmdline.chars()+1 );
                label2.trim();
                if( label2.length() > 0 ){
                    setLabel( label2 );
                    if( label2.compare(label)==0 )
                        return 0;
                }
            }
        }
    }
}

int ScriptShell::addArgv( const NnString &arg )
{
    NnObject *element = arg.clone();
    if( element == NULL )
        return -1;
    return argv_.append( element );
}
