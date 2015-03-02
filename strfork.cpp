#include <stdio.h>
#include "nnstring.h"
#include "nnvector.h"

/*
    [ {A,B}{C,D} ]
    [ A{C,D} ] [ B{C,D} ]
    AC AD BC BD
 */

int strfork(const char *base,NnString &result)
{
    int beforesize=0;
    int quote=0;
    NnString::Iter p(base);

    /* 「{」より左の文字列を、まずはコピー */
    while( *p != '{'  || quote ){
	if( *p == '\0' ){
	    result = base;
	    return 0;
	}else if( *p == '"' ){
	    quote ^= 1;
	}
	++beforesize;
	++p;
    }
    ++p; // skip '{'

    NnVector array;
    NnString *one=new NnString(base,beforesize);

    /* 「}」までの文字列をコピー */
    for(;;){
	if( *p == '}' && quote==0 )
	    break;
	if( *p == ','  && quote == 0 ){
	    array.append( one );
	    one = new NnString(base,beforesize);
	}else if( *p == '\0'){
	    delete one;
	    result = base;
	    return -1;
	}else{
	    if( *p == '"' )
		quote ^= 1;
	    *one << (char)*p;
	}
	++p;
    }
    ++p; // skip '}'
    result.erase();

    /* カンマが一つもない時は、{,} 構文ではない */
    if( array.size() == 0 ){
	result = base;
	delete one;
	return 1;
    }

    array.append( one );
    for(int i=0 ; i<array.size() ; ++i ){
	NnString temp = *(NnString*)array.at(i);

	temp << *p << p.next;
	strfork( temp.chars() , *(NnString*)array.at(i) );
	result << *(NnString*)array.at(i) ;
	if( i < array.size() - 1 )
	    result << ' ';
    }
    return array.size();
}

void brace_expand( NnString &s )
{
    NnVector array;

    s.splitTo( array );
    s.erase();
    for(int i=0;i<array.size();i++){
	NnString temp;

	strfork(((NnString*)array.at(i))->chars() , temp );
	s << temp;
	if( i < array.size() - 1 ){
	    s << ' ';
	}
    }
}
