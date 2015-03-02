#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "nnstring.h"
#include "nnvector.h"
#include "history.h"
#include "reader.h"
#include "shell.h"

int History1::compare( const NnSortable &s ) const
{
    const History1 *h=dynamic_cast<const History1 *>( &s );
    if( h == NULL ){
        return -1;
    }
    int rc=this->stamp_.compare( h->stamp_ );
    if( rc == 0 ){
        rc=this->body_.compare( h->body_ );
    }
    return rc;
}

void History1::touch_()
{
    time_t now;

    time( &now );
    struct tm *t = localtime(&now);
    char ymd[128];
    sprintf(ymd,"%04d/%02d/%02d %02d:%02d:%02d" ,
        t->tm_year + 1900 , t->tm_mon + 1 , t->tm_mday ,
        t->tm_hour , t->tm_min , t->tm_sec );

    this->stamp_ = ymd;
}

/* ヒストリの中から、特定文字列を含んだ行を取り出す
 *	key - 検索キー
 *	line - ヒットした行
 * return 0 - 見つかった , -1 - 見つからなかった.
 */
int History::seekLineHas( const NnString &key , History1 &line )
{
    History1 temp;
    for( int j=size()-2 ; j >= 0 ; --j ){
	this->get( j , temp );
	if( strstr(temp.body().chars(),key.chars()) != NULL ){
	    line = temp;
	    return 0;
	}
    }
    return -1;
}
/* ヒストリの中から、特定文字列から始まった行を取り出す
 *	key - 検索キー
 *	line - ヒットした行
 * return 0 - 見つかった , -1 - 見つからなかった.
 */
int History::seekLineStartsWith( const NnString &key , History1 &line )
{
    History1 temp;
    for( int j=size()-2 ; j >= 0 ; --j ){
	this->get( j , temp );
	if( strncmp(temp.body().chars(),key.chars(),key.length())==0 ){
	    line = temp;
	    return 0;
	}
    }
    return -1;
}

/* 文字列からヒストリ記号を検出して、置換を行う。
 *      hisObj - ヒストリオブジェクト
 *      src - 元文字列
 *      dst - 置換結果を入れる先 or エラー文字列
 * return
 *	0 - 正常終了 , -1 - エラー
 */
int preprocessHistory( History &hisObj , const NnString &src , NnString &dst )
{
    History1 line;
    int quote=0;
    const char *sp=src.chars();

    while( *sp != '\0' ){
	if( *sp != '!' || quote ){
            if( *sp == '"' )
                quote = !quote;
            dst += *sp++;
        }else{
            NnVector argv;
            NnString *arg1;

	    if( *++sp == '\0' )
                break;
            
            int sign=0;
            switch( *sp ){
            case '!':
                ++sp;
            case '*':case ':':case '$':case '^':
		if( hisObj.size() >= 2 ){
		    hisObj.get(-2,line);
                }else{
                    line = "";
		}
                break;
            case '-':
                sign = -1;
		++sp;
            default:
                if( isDigit(*sp) ){
		    int number = (int)strtol(sp,(char**)&sp,10);
		    hisObj.get( sign ? -number-1 : +number , line );
                }else if( *sp == '?' ){
                    NnString key,temp;
                    while( *sp != '\0' ){
                        if( *sp == '?' ){
			    ++sp;
                            break;
                        }
                        key += *sp;
                    }
		    if( hisObj.seekLineHas(key,line) != 0 ){
			dst.erase();
			dst << "!?" << key << ": event not found.\n";
			return -1;
		    }
                }else{
                    NnString key;
		    NyadosShell::readWord(sp,key);

		    if( hisObj.seekLineStartsWith(key,line) != 0 ){
			dst.erase();
			dst << "!" << key << ": event not found.\n";
			return -1;
		    }
                }
                break;
            }
	    line.body().splitTo( argv );

            if( *sp == ':' )
                ++sp;

            switch( *sp ){
            case '^':
                if( argv.size() > 1 && (arg1=(NnString*)argv.at(1)) != NULL )
                    dst += *arg1;
		++sp;
                break;
            case '$':
                if( argv.size() > 0 
                    && (arg1=(NnString*)argv.at(argv.size()-1)) != NULL )
                    dst += *arg1;
		++sp;
                break;
            case '*':
                for(int j=1;j<argv.size();j++){
                    arg1 = (NnString*)argv.at( j );
                    if( arg1 != NULL )
                        dst += *arg1;
                    dst += ' ';
                }
		++sp;
                break;
            default:
                if( isDigit(*sp) ){
		    int n=strtol(sp,(char**)&sp,10);
                    if( argv.size() >= n && (arg1=(NnString*)argv.at(n)) != NULL )
                        dst += *arg1;
                }else{
                    dst += line.body();
                }
                break;
            }
        }
    }
    return 0;
}

History1 *History::operator[](int at)
{
    if( at >= size() )
	return NULL;
    if( at >= 0 )
        return (History1 *)this->at(at);
    /* at が負の場合 */
    at += size();
    if( at < 0 )
	return NULL;
    return (History1 *)this->at( at );
}

void History::pack()
{
    History1 *r1=(*this)[-1];
    History1 *r2=(*this)[-2];
    if( size() >= 2 &&
            r1 != NULL &&
            r2 != NULL &&
            r1->body().compare( r2->body() ) == 0 )
    {
	delete pop();
    }
}

History &History::operator << ( const NnString &str )
{
    this->append( new History1( str ) );
    return *this;
}

void History::read( Reader &reader )
{
    NnString buffer;
    while( reader.readLine(buffer) >= 0 ){
        if(     isdigit(buffer.at(0) & 255) && // YYYY
                isdigit(buffer.at(1) & 255) && 
                isdigit(buffer.at(2) & 255) && 
                isdigit(buffer.at(3) & 255) && 
                buffer.at(4) == '/' &&
                isdigit(buffer.at(5) & 255) && // MM
                isdigit(buffer.at(6) & 255) && 
                buffer.at(7) == '/' &&
                isdigit(buffer.at(8) & 255) && // DD
                isdigit(buffer.at(9) & 255) && 
                buffer.at(10) == ' ' &&
                isdigit(buffer.at(11) & 255) && // HH
                isdigit(buffer.at(12) & 255) && 
                buffer.at(13) == ':' &&
                isdigit(buffer.at(14) & 255) && // MI
                isdigit(buffer.at(15) & 255) && 
                buffer.at(16) == ':' &&
                isdigit(buffer.at(17) & 255) && // SS
                isdigit(buffer.at(18) & 255) && 
                buffer.at(19) == ' ' )
        {
            char stamp[20];
            memcpy( stamp , buffer.chars() , 19 );
            stamp[19] = '\0';
            this->append( new History1(buffer.chars()+20,stamp) );
        }else{
            this->append( new History1(buffer, "1970/01/01 00:00:00") );
        }
    }
}

int History::get(int at,History1 &dst)
{
    History1 *src=(*this)[at];
    if( src == NULL )
        return -1;
    
    dst = *src;
    return 0;
}
int History::set(int at,NnString &str)
{ 
    History1 *dst=(*this)[at];
    if( dst == NULL )
        return -1;
    
    *dst = str;
    return 0;
}
