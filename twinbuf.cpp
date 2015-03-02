#include <ctype.h>
#include <assert.h>
#include <string.h>
#include "getline.h"

enum{
    UNIT_BUFSIZE = 40 ,
    INIT_BUFSIZE = 160,
};

/* 下端ルーチン */
int TwinBuffer::makeroom(int at,int size)
{
    /* バッファサイズが小さいようであれば、広げる */
    if( len+size > max ){
        max = len+size+UNIT_BUFSIZE;
        char *tmp = (char*)realloc( strbuf, max+1 );
        if( tmp == NULL )
            return -1;
        strbuf = tmp;
        tmp = (char*)realloc( atrbuf, max+1 );
        if( tmp == NULL )
            return -1;
        atrbuf = tmp;
    }
    for(int i=len-1 ; i >= at ; i-- ){
        strbuf[ i+size ] = strbuf[ i ];
        atrbuf[ i+size ] = atrbuf[ i ];
    }
    len += size;
    strbuf[ len ] = '\0';
    return 0;
}

void TwinBuffer::delroom(int at,int size)
{
    for(int i=at;i<len;i++){
        strbuf[ i ] = strbuf[ i+size ];
        atrbuf[ i ] = atrbuf[ i+size ];
    }
    len -= size;
    strbuf[ len ] = '\0';
}

/* １文字挿入
 *      key - 文字(>512:倍角文字とみなす)
 *      pos - 挿入桁位置
 * return
 *      挿入バイト数(0～2) : 0 の時はエラー
 */
int TwinBuffer::insert1(int key,int pos)
{
    int upperbyte=(key >> 8) & 255;

    if( upperbyte == 1 ){
        return 0;
    }else if( isCtrl(key) ){
	if( makeroom(pos,2) == -1 )
	    return 0;
	strbuf[ pos   ] = '^';
	atrbuf[ pos   ] = DBC1ST;

	strbuf[ pos+1 ] = '@'+key;
	atrbuf[ pos+1 ] = DBC2ND;

	return 2;
    }else if( upperbyte != 0 ){
        /* DBCS文字 */
        if( makeroom(pos,2) == -1 )
            return 0;
        strbuf[ pos   ] = upperbyte;
        atrbuf[ pos   ] = DBC1ST;
        
        strbuf[ pos+1 ] = key;
        atrbuf[ pos+1 ] = DBC2ND;

        return 2;
    }else{
        if( makeroom(pos,1) == -1 )
            return 0;
        strbuf[ pos ] = key;
        atrbuf[ pos ] = SBC;

        return 1;
    }
}

/* １文字削除を行う。
 *      at - 削除する桁位置
 * return
 *      削除バイト数
 */
int TwinBuffer::erase1(int at)
{
    if( at >= len )
        return 0;
    int siz=sizeAt(at);
    delroom(at,siz);
    return siz;
}

/* at 桁目以降を削除する。
 *      at - 削除開始桁位置
 */
void TwinBuffer::erase_line(int at)
{
    assert( at <= max );
    strbuf[len=at] = '\0';
}

/* 文字列 x を、TwinBuffer に格納する際に必要になるサイズ(桁数)を算出する。
 * 通常、制御文字が2桁と数えられる他は byte と同じ。
 */
int TwinBuffer::strlen_ctrl(const char *x)
{
    int len=0;
    while( *x != '\0'){
	if( isCtrl(*x) )
	    len++;
	len++;
	++x;
    }
    return len;
}

/* 文字列 s を at 桁目に挿入する。
 *      s - 挿入文字列
 *      at - 挿入位置
 * return 
 *      挿入したバイト数
 */
int TwinBuffer::insert(const char *s,int at)
{
    if( s == NULL )
        return 0;

    int len=strlen_ctrl(s);
    if( makeroom( at , len ) == -1 )
        return 0;

    int afterkanji=0;

    while( *s != '\0' ){
        if( afterkanji ){
            atrbuf[ at ] = DBC2ND;
            afterkanji = 0;
	    strbuf[ at++ ] = *s++;
        }else if( isKanji(*s) ){
            atrbuf[ at ] = DBC1ST;
            afterkanji = 1;
	    strbuf[ at++ ] = *s++;
        }else if( isCtrl(*s) ){
	    atrbuf[ at   ] = DBC1ST;
	    strbuf[ at++ ] = '^';
	    atrbuf[ at   ] = DBC2ND;
	    strbuf[ at++ ] = '@'+*s++;
	    afterkanji = 0;
	}else{
            atrbuf[ at ] = SBC;
            afterkanji = 0;
	    /* 改行の類は空白に直してしまう */
	    if( *s == '\n' || *s == '\r' ){
		strbuf[ at++ ] = ' ';
		++s;
	    }else{
		strbuf[ at++ ] = *s++;
	    }
	}
    }
    return len;
}

/* 初期化 
 * return 0…成功 , -1…失敗
 */
int TwinBuffer::init()
{
    strbuf = (char*)malloc( INIT_BUFSIZE+1 );
    if( strbuf == NULL )
        return -1;

    atrbuf = (char*)malloc( INIT_BUFSIZE+1 );
    if( atrbuf == NULL ){
        free(strbuf);
        strbuf = NULL;
        return -1;
    }
    max = INIT_BUFSIZE;
    len = 0 ;
    strbuf[ len ] = '\0';
    return 0;
}

void TwinBuffer::term()
{
    free(strbuf);strbuf=NULL;
    free(atrbuf);atrbuf=NULL;
    max = len = 0;
}


/* at から nchars バイト分を string で置き換る
 * return
 *      置きかえることによる増減値。
 *      エラー時：EXCEPTIONS(-32768)
 */
int TwinBuffer::replace(int at,int nchars, const char *string )
{
    int new_nchars=strlen_ctrl(string);

    if( nchars < new_nchars ){
        if( makeroom( at+nchars , new_nchars-nchars ) == -1 )
            return EXCEPTIONS;
    }else if( nchars > new_nchars ){
        delroom( at+new_nchars , nchars-new_nchars);
    }

    int j=0,i=0;
    while( i < new_nchars ){
        if( isKanji( string[i] ) && i<new_nchars-1 ){
            strbuf[ at+j   ] = string[i++];
            atrbuf[ at+j++ ] = DBC1ST;
            strbuf[ at+j   ] = string[i++];
            atrbuf[ at+j++ ] = DBC2ND;
	}else if( isCtrl( string[i] ) ){
	    strbuf[ at+j   ] = '^';
	    atrbuf[ at+j++ ] = DBC1ST;
	    strbuf[ at+j   ] = '@' + string[i++];
	    atrbuf[ at+j++ ] = DBC2ND;
        }else{
            strbuf[ at+j   ] = string[i++];
            atrbuf[ at+j++ ] = SBC;
        }
    }
    return j-nchars;
}

/* ^x 形式の Ctrl コードをデコードする.
 *	at - 読み出し開始位置
 *	len - 読み出しサイズ
 *	buffer - 格納先
 * return
 *	デコード後のサイズ.
 */
int TwinBuffer::decode(int at , int len , NnString &buffer )
{
    buffer.erase();
    for(int i=0;i<len;i++){
	if( strbuf[at+i]=='^' &&  atrbuf[at+i]==DBC1ST ){
	    buffer << (char)(strbuf[at+ ++i] & 0x1F);
	}else{
	    buffer << (char)strbuf[at+i];
	}
    }
    return buffer.length();
}
int TwinBuffer::decode(NnString &buffer)
{
    return decode(0,this->length(),buffer);
}
