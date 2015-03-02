#ifndef SHELL_H
#define SHELL_H

#include <stdio.h>
#include "const.h"
#include "nnhash.h"
#include "nnvector.h"
#include "reader.h"
#include "writer.h"
#include "history.h"

class NyadosShell : public NnObject {
    enum { MAX_NESTING = 50 };
    static NnHash command;

    NnString current;
    NnString heredocfn;
    int exitStatus_;

    void doHereDocument( const char *&sp , NnString &dst , char prefix );
    void transPercent( const char *&sp , NnString &dst , int level );
    int explode4external( const NnString &src , NnString &dst );
    int interpret1( const NnString &statement ); /* && || */
    int interpret2( const NnString &statement , int wait ); /* pipeline */
protected:
    NyadosShell *parent_;
public:
    static int explode4internal( const NnString &src , NnString &dst );

    NnVector nesting;
    NnString tempfilename;

    int exitStatus(){ return exitStatus_; }
    void setExitStatus(int n);

    virtual Status readline(NnString &line)=0;
    virtual History *getHistoryObject(){ return NULL; }
    Status readcommand(NnString &cmd);

    int interpret( const NnString &statement );
    int mainloop();

    virtual int operator !() const=0;
    virtual operator const void *() const
	{ return !*this ? 0 : this; }

    virtual ~NyadosShell();
    virtual int setLabel( NnString & );
    virtual int goLabel( NnString & );
    virtual int argc() const;
    virtual const NnString *argv(int) const;
    virtual void shift();
    virtual const char *className() const =0;
    const char *repr() const { return this->className(); }

    NyadosShell( NyadosShell *parent = 0 );

    static int readWord( const char *&ptr , NnString &token );
    static int skipSpc( const char *&ptr );
    static void readNextWord( const char *&ptr , NnString &token );
    static void dequote( const char *p , NnString &result );
    static void dequote( NnString &result );
    static void enquote( const char *buf , NnString &dst );
};

class ScriptShell : public NyadosShell {
    StreamReader *fr;
    NnHash labels;
    NnVector  argv_;
public:
    Status readline(NnString &line)
    { return fr->readLine(line) < 0 ? TERMINATE : NEXTLINE ; }
    ScriptShell() : fr(0){}
    ScriptShell( StreamReader *reader ) : fr(reader){}
    ScriptShell( const char *fname ) : fr(new FileReader(fname)){}
    ~ScriptShell(){ delete fr; }
    int open( const char *fname )
        { return (fr=new FileReader(fname)) == 0 || fr->eof() ; }
    int operator !() const { return fr==0 || fr->eof(); }

    virtual int setLabel( NnString &label );
    virtual int goLabel( NnString &label );
    int addArgv( const NnString &arg );
    virtual int argc() const { return argv_.size(); }
    virtual const NnString *argv(int n) const;
    virtual void shift();
    const char *className() const { return "ScriptShell"; }
};
struct LabelOnScript : public NnObject { fpos_t pos; };

class OneLineShell : public NyadosShell {
    NnString cmdline;
public:
    OneLineShell( NyadosShell *p , NnString &s ) : NyadosShell(p) , cmdline(s) {}
    OneLineShell( const char *s ) : cmdline(s){}
    Status readline( NnString &buffer );
    int operator !() const;
    const char *className() const { return "OneLineShell"; }
};

class BufferedShell : public NyadosShell {
    NnVector buffers;
    NnVector *params;
    int count;
protected:
    virtual Status readline( NnString &line );
public:
    BufferedShell() : params(0), count(0){}
    ~BufferedShell()
	{ delete params; }
    virtual int operator !() const
        { return count >= buffers.size(); }

    void append( NnString *list )
	{ buffers.append(list); }
    void rewind()
	{ count=0; }
    const char *className() const 
	{ return "BufferedShell"; }

    virtual int argc() const 
	{ return params != 0 ? params->size() : 0 ; }
    virtual const NnString *argv(int n) const;
    void setArgv( NnVector *p )
	{ delete params; params = p; ; rewind(); }
    const NnString &statement(int n){ return *(NnString*)buffers.at(n); }
    int size(){ return buffers.size(); }
};

class NnStrFilter {
protected:
    virtual void filter( NnString::Iter &p , NnString &result )=0;
public:
    NnStrFilter(){}
    void operator()( NnString &target );
};
/* 環境変数・シェル変数展開用フィルター */
class VariableFilter : public NnStrFilter {
    NyadosShell &shell;

    void cnv_digit( NnString::Iter & , NnString & );
    void cnv_asterisk( NnString::Iter & , NnString & );
protected:
    int lookup( NnString &name , NnString &value );
    void filter( NnString::Iter &p , NnString &result );
public:
    VariableFilter( NyadosShell &shell_ ) : shell(shell_){}
};

#endif
