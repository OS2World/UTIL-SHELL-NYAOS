#include <stdlib.h>
#include "nnstring.h"
#include "shell.h"

/** buf の中に空白があれば、外側を引用符で囲む。
 *	buf - 元文字列
 *	dst - 変換後文字列
 */
void NyadosShell::enquote( const char *buf , NnString &dst )
{
    if( strchr(buf,' ') != NULL ){
	dst << '"' << buf << '"';
    }else{
	dst << buf ;
    }
}

/** 文字列 p から、引用符を除く。ただし、"" は " に変換する。
 *	p - 元文字列
 *	result - 変換後文字列
 */
void NyadosShell::dequote( const char *p , NnString &result )
{
    result.erase();
    int quotecount=0;
    for( ; *p != '\0' ; ++p ){
	if( *p == '"' ){
	    if( ++quotecount % 2 == 0 )
		result << '"';
	}else if( *p == '\n' ){
	    result << "$T";
	    quotecount = 0;
	}else{
	    result << (char)*p;
	    quotecount = 0;
	}
    }
}

void NyadosShell::dequote( NnString &result )
{
    NnString result_;

    dequote( result.chars() , result_ );
    result = result_ ;
}


/* 文字列を読み取る。
 * 引用符なども認識する。
 *      sp - 文字列先頭ポインタ
 *            →実行後は、末尾の次の文字位置(空白,\0 位置へ)
 *      token - 文字列を入れる先
 * return
 *      0 - 普通
 *      '<', '>','|'
 *      EOF - 末尾
 */
int NyadosShell::readWord( const char *&sp , NnString &token )
{
    int quote=0;
    token.erase();
    for(;;){
        if( *sp == '\0' )
            return EOF;
        if( isSpace(*sp) && quote==0 )
            return 0;
        if( quote==0  &&  (*sp=='<' || *sp=='>' || *sp=='|' ) )
            return *sp;

        if( quote==0  &&  *sp=='%' ){
            NnString envname;
            for(;;){
                ++sp;
                if( *sp == '\0' ){
                    token << '%' << envname;
                    return EOF;
                }
                if( *sp == '<' || *sp == '>' || *sp == '|' ){
                    token << '%' << envname;
                    return *sp;
                }
                if( *sp == '"' ){
                    quote = !quote;
                    token << '%' << envname;
                    break;
                }
                if( *sp == '%' ){
                    const char *envval=getEnv( envname.chars() , NULL );
                    if( envval == NULL ){
                        token << '%' << envname << '%';
                    }else{
                        token << envval;
                    }
                    break;
                }
                envname << *sp;
            }
        }else{
	    if( *sp == '"' )
		quote = !quote;
	    else
		token << *sp;
        }
        ++sp;
    }
}
/* スペース文字をスキップする
 *	sp : 変換前後の文字列
 * return
 *	0 : 成功
 *	-1 : スペース以外の文字が見つからなかった。
 */
int NyadosShell::skipSpc( const char *&sp )
{
    for(;;){
        if( *sp == '\0' )
            return -1;
        if( !isSpace(*sp) )
            return 0;
        ++sp;
    }
}

/* リダイレクト先を読み取る。
 *      sp - '>' などの文字の次を指しているポインタ
 *           → 実行後は末尾の次の文字位置へ(空白,\0)
 *      fn - ファイル名
 */
void NyadosShell::readNextWord( const char *&sp , NnString &fn )
{
    if( skipSpc( sp ) == -1 ){
	fn.erase();
	return ;
    }
    readWord( sp , fn );
    fn.slash2yen();
}
