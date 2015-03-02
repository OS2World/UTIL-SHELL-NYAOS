#ifndef NSTRING_H
#define NSTRING_H

#include <ctype.h>
#include <string.h>
#include "nnobject.h"

class NnVector;

/* letter_t :
 *   上位 8bit = {0,-1} :  8bit文字(下位8bitのみ有効)
 *               その他 : 16bit文字(全bit有効)
 */
typedef unsigned short letter_t;

/** NnString :
 *  ・コンパクト指向の文字列オブジェクト。
 *
 *  ・リファレンスカウンティングで、複数インスタンスにおいても、
 *    同一文字列を共有している。
 *
 *  ・一時オブジェクトの扱いが面倒な二項演算子(+)は用意していない。
 *    かわりに、代入演算子(+=)、挿入演算子(<<)を使用することができる。
 *    (JavaのStringBufferに近い)
 *
 *  ・const char* への変換は、明示的に chars() メソッドを使って行う。
 *    キャスト演算子の多重定義は行っていない。
 *
 *  ・NnHash のキーとして用いることができる唯一のクラス。
 */
class NnString : public NnSortable { 
    struct Rep {
	int refcnt;
	int length;
	int max;
	char  buffer[1];
    };
    static Rep zero;
    enum{ INCSIZ = 16 };
    Rep *rep;
    int set( const char *s );
    int set( const char *s , int size);
    static void reset(Rep *&r);
    int independ(void);
    int keep( int len ); 
    int grow( int len ); 

    static int f2b_(int ch);
    static int b2f_(int ch);
    static int deq_(int ch);
public:
    virtual ~NnString();
    /** オブジェクト object と比較し、大小関係を負・零・正の数値で返す */
    virtual int compare( const NnSortable &object ) const;
    const char *repr() const { return this->chars(); }

    /** 空で初期化する */
    NnString() : rep(&zero) {}
    /** 文字列 s で初期化する */
    NnString( const char *s ){ set(s); }
    /** ポインタ s よりの size バイトのバイト列で初期化する */
    NnString( const char *s , int size ){ set(s,size); }
    /** 文字列 ns で初期化する */
    NnString( const NnString &ns );

    // 代入演算子.
    /** ポインタ s よりの size バイトのバイト列を代入する */
    int assign( const char *s , int size );
    /** 文字列 ns を代入する */
    NnString &operator = ( const NnString &ns );
    /** 文字列 x を代入する */
    NnString &operator = ( const char *x );
    /** 文字 c を代入する */
    NnString &operator = ( char c ){ assign( &c , 1 ); return *this; }

    NnString &insertAt( int at , const char *s , int siz);
    NnString &insertAt( int at , const char *s )
	{ return insertAt(at,s,strlen(s) ); }
    NnString &insertAt( int at , const NnString &ns )
	{ return insertAt(at,ns.chars(),ns.length() ); }

    /** 文字 c を末尾に追加する */
    NnString &operator += ( char c );
    /** 文字列 s を末尾に追加する */
    NnString &operator += ( const char *s )
	{ return insertAt( length(), s, strlen(s) ); }
    /** 文字列 ns を末尾に追加する */
    NnString &operator += ( const NnString &ns )
	{ return insertAt( length(), ns ); }

    /** 文字 c を末尾に追加する */
    NnString &operator << ( char c ){ return *this += c ; }
    /** 文字列 s を末尾に追加する */
    NnString &operator << ( const char *s ){ return *this += s; }
    /** 文字列 ns を末尾に追加する */
    NnString &operator << ( const NnString &ns )
	{ return insertAt( length() , ns ); }
    /** 数値を 10進文字列へ変換して末尾に追加する */
    NnString &addValueOf( int x );
    
    /* 漢字１文字 */
    NnString &operator << (letter_t s)
	{ *this += (char)(s>>8) ; return *this += (char)(s & 255); }

    // リポート関数.
    /** char*型文字列を取り出す */
    const char *chars()  const { return rep->buffer; }
    /** 空文字であれば 真を返す */
    int         empty()  const { return rep->length <= 0 ; }
    /** 文字列の長さを得る */
    int         length() const { return rep->length; }
    /** nバイト目の文字を得る */
    char        at(int n) const { return rep->buffer[n]; }
    char        &operator[]( int n ) { return rep->buffer[n]; }
    /** 文字列を空にする */
    void        erase(){ reset(rep); }

    /** 文字列 s と比較し、大小関係を負・零・正の数値で返す */
    virtual int compare( const char *s ) const;
    /** 文字列 s と比較し、大小関係を負・零・正の数値で返す
     *  英大文字・小文字を区別しない */
    int icompare( const char *s ) const;
    /** 文字列 s と比較し、大小関係を負・零・正の数値で返す
     *  英大文字・小文字を区別しない */
    int icompare( const NnString &ns ) const;
    
    /** 文字列の前後の空白を削除する */
    NnString &trim();
    /** 末尾の１バイトを削除する */
    void chop();
    /** 文字列を len バイトにしてしまう(切る) */
    void chop(int len);
    /** 先頭の１バイトを削除する */
    void shift();
    /** 先頭に１バイトを挿入する */
    void unshift( char ch );

    void filter( int (*func)(int) );
#ifdef __DMC__
    static int toLower(int x){ return tolower(x); }
    static int toUpper(int x){ return toupper(x); }
    void downcase(){ filter(toLower); }
    void upcase(){ filter(toUpper); }
#else
    void downcase(){ filter(tolower); }
    void upcase(){ filter(toupper); }
#endif
    void slash2yen(){ filter(f2b_); }
    void yen2slash(){ filter(b2f_); }
    void dequote(){   filter(deq_); }

    /** 文字列を空白で分割する */
    void splitTo( NnString &first , NnString &rest ) const ;
    int splitTo( NnString &first , NnString &rest , const char *dem , const char *quote="\"" , const char *escape="^" ) const;
    int splitTo( NnVector &vec ) const;

    /** 末尾の１文字(DBCS意識)の上位８ビットを返す */
    int lastchar() const;

    /** s で始まっていれば 真を返す */
    int   startsWith( const char *s     ) const;
    int   startsWith( const NnString &s ) const;
    /** s で始まっていれば、真を返す。英大文字・小文字を区別しない */
    int  istartsWith( const char *s     ) const;
    int  istartsWith( const NnString &s ) const;

    /** s で終わっていれば、真を返す。*/
    int     endsWith( const char *s     ) const;
    int     endsWith( const NnString &s ) const;
    /** s で終わっていれば、真を返す。英大文字・小文字を区別しない。*/
    int    iendsWith( const char *s     ) const;
    int    iendsWith( const NnString &s ) const;
    /* func で比較した結果、s で終わっていれば、真を返す */
#ifdef __DMC__
    int    endsWith( const char *s , int __CLIB func(const char *,const char *) ) const;
#else
    int    endsWith( const char *s , int func(const char *,const char *) ) const;
#endif
    
    /** ハッシュ値を返す */
    virtual unsigned hashValue() const;
    static unsigned hashValue(const char *s);
    virtual NnObject *clone() const;

    static int findOf(     const char *p,const char *dem,int startIndex=0 );
    static int findLastOf( const char *p,const char *dem,int startIndex=0 );
    int findOf( const char *dem,int startIndex=0 ) const
	{ return findOf( chars() , dem , startIndex ); }
    int findLastOf( const char *dem , int startIndex=0 ) const
	{ return findLastOf( chars() , dem , startIndex ); }
    int search( const NnString &target , int start=0 );
    void replace( const NnString &from , const NnString &to , NnString &result );
    
    /* 1文字何で、文字列を走査するポインタ */
    class Iter{
	letter_t last;
    public:
	const char *next; /* 次の位置を指定するポインタは公開してしまう */

	void operator ++ (){
	    if( isKanji(*next) ){
		last = ((next[0]<<8) | (next[1] & 255));
		next += 2;
	    }else if( *next ){
		last = *next++;
	    }else{
		last = 0;
	    }
	}
	void operator ++(int){ ++*this; }
	letter_t operator * (){ return last; }
	Iter( const char *p_    ) : next( p_ ){ ++*this; }
	Iter( const NnString &s ) : next( s.chars() ){ ++*this; }
	Iter( const Iter &i     ) : last(i.last),next( i.next ){}
	int space(){ return last < 256 && isspace(last); }
	int alpha(){ return last < 256 && isalpha(last); }
	int alnum(){ return last < 256 && isalnum(last); }
	int digit(){ return last < 256 && isdigit(last); }
    };
};

/* NnString と同じだが、compare/sort/uniq実施の際に、大文字・小文字を
 * 区別しない。*/
class NnStringIC : public NnString {
public:
    NnStringIC( const char *s ) : NnString(s){}
    NnStringIC( const NnString &ns ) : NnString(ns){}
    NnStringIC( const char *s , int size ) : NnString(s,size){}
    NnStringIC( const NnStringIC &ns ) : NnString(ns){}
    
    virtual int compare( const char *s ) const ;
    virtual int compare( const NnSortable &object ) const;
    virtual unsigned hashValue() const;
};

#endif
