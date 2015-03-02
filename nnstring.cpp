/* Nihongo Nano Class Library `NnString' -- 文字列クラス実装ソース */
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <limits.h>
#include "nnstring.h"
#include "nnvector.h"

NnObject::~NnObject(){}
NnObject *NnObject::clone() const { return 0; }
NnObject *NnString::clone() const { return new NnString(*this); }

NnSortable *NnObject::sortable(){ return 0; }
NnSortable *NnSortable::sortable(){ return this; }

int NnEnum::more(){ return **this != NULL; }

NnString::Rep NnString::zero={ 1,0,1, {'\0'} };
#define ZEROSTR &zero

void NnString::reset(NnString::Rep *&r)
{
    assert( r != NULL );
    assert( zero.buffer[0] == '\0' );

    if( r != ZEROSTR && --(r->refcnt) <= 0 ){
	free(r);
    }
    r = ZEROSTR;
}

NnString::NnString( const NnString &ns )
{
    assert( zero.buffer[0] == '\0' );
    if( (rep=ns.rep) != ZEROSTR )
	rep->refcnt++;
}

NnString::~NnString()
{
    assert( zero.buffer[0] == '\0' );
    assert( zero.refcnt == 1 );
    reset(rep);
}

/* 代入演算子(公開) */
NnString &NnString::operator = ( const NnString &ns )
{
    Rep *old=rep;
    if( (rep=ns.rep) != ZEROSTR )
	rep->refcnt++;
    reset( old );
    return *this;
}

NnString &NnString::operator = ( const char *s )
{
    Rep *old=rep;
    if( set(s) == 0 )
	reset(old);
    return *this;
}

/* メモリ領域(アドレス＆サイズ)を文字列に代入.
 *      s - 先頭アドレス
 *      size - サイズ
 * return
 *       0 - 代入成功
 *      -1 - メモリエラー 
 */
int NnString::assign( const char *s , int size )
{
    Rep *old=rep;
    if( set(s,size) == 0 ){
	reset(old);
        return 0;
    }else{
        return -1;
    }
}

/* 文字列 s の内容をインスタンスに設定する。
 *	s 元文字列
 * return
 *	0 : 成功 , -1 : 失敗
 */
int NnString::set( const char *s )
{
    int len;
    if( s != NULL && (len=strlen(s)) > 0 ){ 
	return set( s , len );
    }else{
	rep = ZEROSTR;
	assert( rep->buffer[0] == '\0' );
	return 0;
    }
}
/* s ～ s+len までの領域をインスタンスに設定する。
 *	s 元文字列の先頭アドレス
 *	len 元文字列の長さ
 * return
 *	0 : 成功 , -1 : 失敗
 */
int NnString::set( const char *s , int len )
{
    if( s != NULL && len > 0 ){
        int max1=len+INCSIZ;

	rep = (Rep *)malloc(sizeof(Rep) + max1 );
	if( rep == NULL ){
	    rep = ZEROSTR;
	    return -1;
	}

	rep->refcnt = 1;
	rep->length = len;
	rep->max    = max1;
	memcpy( rep->buffer , s , len );
	rep->buffer[ len ] = '\0';
    }else{
	rep = ZEROSTR;
    }
    return 0;
}

/* もし文字列本体が他のインスタンスと共用していたら、
 * 独立させ、文字列本体を変更可能にする。
 * return
 *	0 : 成功 , -1 : 失敗
 */
int NnString::independ()
{
    assert( zero.buffer[0] == '\0' );
    if( rep != ZEROSTR && rep->refcnt >= 2 ){
	Rep *old=rep;
	if( set( old->buffer ) != 0 )
	    return -1;
	reset( old );
    }
    return 0;
}

/* LEN文字分追加しても可能なように領域を確保する。
 * 確保済みの領域で足りる場合は何もしない。
 *	len 増加文字数
 * return
 *	0 : 成功 , -1 : 失敗
 */
int NnString::grow( int growsize )
{
    return keep( rep->length + growsize );
}

/* 最低 len 文字入るだけの領域を確保する
 *      len 確保文字数
 * return
 *      0 : 成功 , -1 : メモリーエラー
 */
int NnString::keep( int len )
{
    if( len < 0 || len >= INT_MAX ){
        return -1;
    }
    /* keep を実行する
     * → 文字列に変更を加える.
     * → 複製化しなければいけない
     */
    if( independ() != 0 )
        return -1;

    if( len <= rep->max )
        return 0;
    
    // 孤立している場合で、容量が足りない場合は足す.
    int newmax = len + INCSIZ ;
    assert( rep != NULL );
    Rep *newrep = (Rep*)( rep != ZEROSTR ? realloc( rep, sizeof(Rep)+newmax )
				         :  malloc(      sizeof(Rep)+newmax ) );
    if( newrep == NULL )
        return -1;
    rep = newrep;
    rep->max = newmax;

    return 0;
}

NnString &NnString::insertAt( int at , const char *s , int siz)
{
    if( s == NULL || siz == 0 ) return *this;
    if( rep == ZEROSTR ){ assign(s,siz); return *this; }

    if( grow(siz) != 0 || independ() != 0 )
	return *this;
    
    for(int i=rep->length ; i >= at ; --i )
	rep->buffer[ i+siz ] = rep->buffer[ i ];
    
    memcpy( &rep->buffer[ at ] , s , siz );
    rep->length += siz;
    return *this;
}

NnString &NnString::operator += ( char c )
{
    /* '\0' や -1 を無視するのは、これらは１バイト文字が
     * ２バイトコードに拡張されたときの上位バイトとなりえる
     * コードだから。これを無視させることで、operator << 
     * (letter_t) の実装が簡単になっている。
     */
    if( c == '\0' || c == -1 )
	return *this;

    if( rep == ZEROSTR ){
	assign( &c , 1 );
    }else{
	if( grow(1) == 0 ){
	    rep->buffer[ rep->length++ ] = c;
	    rep->buffer[ rep->length   ] = '\0';
	}
    }
    return *this;
}

int NnString::compare( const NnSortable &object ) const
{
    const NnString *another=(const NnString*)&object;
    return strcmp( chars() , another != NULL ? another->chars() : "" );
}

unsigned NnString::hashValue(const char *p)
{
    unsigned sum=0;
    while( *p != '\0' ){
	sum <<= 1;
	sum ^= *p++;
    }
    return sum;
}

unsigned NnString::hashValue() const
{
    return hashValue( rep->buffer );
}

NnString &NnString::trim()
{
    if( rep==ZEROSTR )
	return *this;
    
    int i,j;
    independ();

    /* 後ろのスペースを除く */
    for( i=length()-1 ; i >= 0  && isSpace(rep->buffer[i]) ; --i )
	;
    rep->buffer[ rep->length = i+1] = '\0';

    /* 前のスペースを除く */
    for( i=0 ; i < length() && isSpace(rep->buffer[i]) ; i++ )
	;
    if( i >= 1 ){
	for(j=0 ; i < length() ; j++ , i++ )
	    rep->buffer[j] = rep->buffer[i];
	rep->buffer[ rep->length = j ] = '\0';
    }
    return *this;
}
void NnString::shift()
{
    if( length() >= 1 ){
	independ();
	for(int i=0;i<length();i++)
	    rep->buffer[ i ] = rep->buffer[ i+1 ];
	rep->length--;
    }
}
void NnString::chop()
{
    if( length() >= 1 ){
	independ();
	rep->buffer[ --(rep->length) ] = '\0';
    }
}
void NnString::chop(int at)
{
    if( rep != ZEROSTR  &&  length() > at ){
	independ();
	rep->buffer[ rep->length = at ] = '\0';
    }
}

void NnString::unshift( char ch )
{
    if( rep == ZEROSTR ){
	*this = ch;
	return;
    }
    independ();
    if( grow(1) == 0 ){
	for(int i=length() ; i >= 0 ; --i )
	    rep->buffer[i+1] = rep->buffer[i];
	rep->buffer[0] = ch ;
	rep->length++;
    }
}

int NnString::compare( const char *s ) const
{
    return strcmp( chars() , s ? s : "");
}
int NnString::icompare( const char *s ) const
{
    return stricmp( chars() , s ? s : "" );
}
int NnString::icompare( const NnString &ns ) const
{
    return stricmp( chars() , ns.chars() );
}

int NnString::b2f_(int ch){ return ch=='\\' ? '/' : ch; }
int NnString::f2b_(int ch){ return ch=='/' ? '\\' : ch; }
int NnString::deq_(int ch){ return ch=='"' ? '\0' : ch; }

void NnString::filter( int (*func)(int) )
{
    independ();
    assert( rep != NULL );
    if( rep == ZEROSTR )
        return;

    const char *sp=rep->buffer;
    char *dp=rep->buffer;

    while( *sp != '\0' ){
        if( isKanji(*sp) ){
	    *dp++ = *sp++;
	    *dp++ = *sp++;
        }else{
	    *dp=(*func)(*sp++ & 255);
	    if( *dp != '\0' )
		++dp;
        }
    }
    rep->length = (dp - rep->buffer);
    rep->buffer[ rep->length ] = '\0';
}

void NnString::splitTo( NnString &first , NnString &rest ) const
{
    int top=0,p=0;
    while( p < length() && isSpace(at(p)) ){
	++top;
	++p;
    }

    int quote=0;
    int len=0;
    bool escape = false;
    while( p < length() ){
        if( isSpace( at(p) ) && !quote && !escape )
            break;
        if( this->at(p)=='"' )
            quote ^= 1;
        escape = (!quote & !escape && this->at(p)=='^' );
	if( isKanji(this->at(p)) ){
	    ++len;
	    ++p;
	}
        ++len;
	++p;
    }
    first.assign( this->chars()+top , len );
    if( p < this->length() ){
        rest = this->chars() + p ;
        rest.trim();
    }else{
        rest.erase();
    }
}
int NnString::splitTo( NnString &first , NnString &rest , const char *dem , const char *quotechars , const char *escapechars ) const
{
    NnString rest1;
    int i=0 , quote=0 ;
    bool escape = false;
    while( i < length() ){
        const char *q=strchr(quotechars,at(i));
        if( q != NULL && *q != '\0' ){
            quote ^= (1<<(q-quotechars));
        }
        if( !quote && !escape && strchr(dem,at(i)) != NULL ){
            rest1 = chars()+i+1 ;
            break;
        }
        escape = (strchr(escapechars,at(i)) != NULL );
	if( isKanji(at(i)) )
	    ++i;
	++i;
    }
    int rc=at(i);
    first.assign( chars() , i );
    rest = rest1;
    return rc;
}

/* その行が特定キーワードで始まるか判定する。
 * startsWith  … 大文字/小文字を区別する。
 * iStartsWith … 大文字/小文字を区別しない。
 *      line - 行
 *      keyword - キーワード
 * return
 *      not 0 - 含まれる
 *      0 - 含まれない
 */
int NnString::startsWith( const char *s ) const
{
    return strncmp( chars() , s , strlen(s) )==0 ;
}
int NnString::istartsWith( const char *s ) const
{
    return strnicmp( chars() , s , strlen(s) )==0 ;
}

int NnString::endsWith( const char *s , int func(const char *,const char *) ) const
{
    int len1=strlen(s);
    int i;
    if( len1 > length() )
        return 0;

    for(i=0 ; i<length()-len1; ++i ){
        if( isKanji(at(i)) )
            ++i;
    }
    return i==length()-len1 && (*func)(chars()+i,s)==0;

}
/* その行が特定キーワードで終わるか判定する。
 * キーワードは大文字・小文字を問わない
 * endWith  … 大文字/小文字を区別する。
 * iendWith … 大文字/小文字を区別しない。
 *      line - 行
 *      keyword - キーワード
 * return
 *      not 0 - 含まれる
 *      0 - 含まれない
 */
int NnString::endsWith( const char *s ) const
{
    return endsWith( s , &::strcmp );
    // return strcmp(chars()+length()-strlen(s),s)==0;
}

int NnString::iendsWith( const char *s ) const
{
    return endsWith( s , &::stricmp );
    // return stricmp(chars()+length()-strlen(s),s)==0;
}

int NnString::iendsWith( const NnString &s ) const
{
    return endsWith( s.chars() , &stricmp );
    // return iendsWith( s.chars() );
}

int  NnString::istartsWith( const NnString &s ) const
{
    return istartsWith( s.chars() );
}

int  NnString::startsWith( const NnString &s ) const
{
    return startsWith( s.chars() );
}

/* 末尾の文字を返す. 日本語対応.
 */
int  NnString::lastchar() const
{
    if( rep == ZEROSTR )
        return '\0';
    
    int ch=0;
    const char *p=rep->buffer;
    while( *p != '\0' ){
        ch = *p;
        if( isKanji(*p) && *++p == '\0' )
            break;
        ++p;
    }
    return ch;
}
int NnStringIC::compare( const char *s ) const
{
    return this->NnString::icompare( s );
}

int NnStringIC::compare( const NnSortable &object ) const
{
    const NnString *another=(const NnString*)&object;
    return this->NnString::icompare( another != NULL ? another->chars() : "" );
}

unsigned NnStringIC::hashValue() const
{
    unsigned sum=0;
    for( const char *p=this->chars(); *p != '\0' ; ++p ){
        sum <<= 1;
        sum ^= tolower(*p);
    }
    return sum;
}

/* p の i 文字以降で、文字列 dem に含まれる文字が現れる最初の位置を返す.
 * (strcspn とほぼ同じ:日本語対応)
 * return
 *      demに含まれる文字の登場位置 - pの先頭の長さ
 */
int NnString::findOf(const char *p,const char *dem,int i)
{
    while( p[i] != '\0' ){
	int j=0;
	while( dem[j] != '\0' ){
	    if( p[i] == dem[j] ){
		if( isKanji(dem[j]) ){
		    if( p[i+1] == dem[j+1] )
			return i;
		}else{
		    return i;
		}
	    }
	    if( isKanji(dem[j]) )
		++j;
	    ++j;
	}
	if( isKanji(p[i]) )
	    ++i;
	++i;
    }
    return -1;
}

/* p の i 文字以降で、文字列 dem に含まれる文字が現れる最後の位置を返す.
 * return
 *      demに含まれる文字の最後の登場位置 - pの先頭の長さ
 */
int NnString::findLastOf(const char *p,const char *dem,int startIndex)
{
    int result=-1;
    while( (startIndex=NnString::findOf(p,dem,startIndex)) != -1 ){
	result = startIndex;
	++startIndex;
    }
    return result;
}

/* x を10進数文字列へ変換して、末尾に加える. */
NnString &NnString::addValueOf( int x )
{
    if( x < 0 ){
	*this << '-';
	return this->addValueOf( -x );
    }
    if( x >= 10 ){
	this->addValueOf( x/10 );
	x %= 10;
    }
    return *this << "0123456789"[x];
}

int NnString::search( const NnString &target , int i )
{
    const char *p=rep->buffer+i;
    const char *q=target.rep->buffer;
    while( i < this->length() ){
        if( *p == *q && memcmp(p,q,target.length())==0 ){
            return i;
        }
        if( isKanji(*p) ){
            ++i;++p;
        }
        ++i;++p;
    }
    return -1;
}

void NnString::replace( const NnString &from , const NnString &to , NnString &result_ )
{
    int index,lastindex=0;
    NnString result;

    while( (index=this->search(from,lastindex)) >= 0 ){
        for(int i=lastindex ; i<index ; i++){
            result << this->at(i);
        }
        result << to;
        lastindex = index + from.length();
    }
    for(int i=lastindex ; i<this->length() ; i++){
        result << this->at(i);
    }
    result_ = result;
}
