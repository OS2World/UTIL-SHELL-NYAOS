#ifndef NSTRING_H
#define NSTRING_H

#include <ctype.h>
#include <string.h>
#include "nnobject.h"

class NnVector;

/* letter_t :
 *   ��� 8bit = {0,-1} :  8bit����(����8bit�̂ݗL��)
 *               ���̑� : 16bit����(�Sbit�L��)
 */
typedef unsigned short letter_t;

/** NnString :
 *  �E�R���p�N�g�w���̕�����I�u�W�F�N�g�B
 *
 *  �E���t�@�����X�J�E���e�B���O�ŁA�����C���X�^���X�ɂ����Ă��A
 *    ���ꕶ��������L���Ă���B
 *
 *  �E�ꎞ�I�u�W�F�N�g�̈������ʓ|�ȓ񍀉��Z�q(+)�͗p�ӂ��Ă��Ȃ��B
 *    �����ɁA������Z�q(+=)�A�}�����Z�q(<<)���g�p���邱�Ƃ��ł���B
 *    (Java��StringBuffer�ɋ߂�)
 *
 *  �Econst char* �ւ̕ϊ��́A�����I�� chars() ���\�b�h���g���čs���B
 *    �L���X�g���Z�q�̑��d��`�͍s���Ă��Ȃ��B
 *
 *  �ENnHash �̃L�[�Ƃ��ėp���邱�Ƃ��ł���B��̃N���X�B
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
    /** �I�u�W�F�N�g object �Ɣ�r���A�召�֌W�𕉁E��E���̐��l�ŕԂ� */
    virtual int compare( const NnSortable &object ) const;
    const char *repr() const { return this->chars(); }

    /** ��ŏ��������� */
    NnString() : rep(&zero) {}
    /** ������ s �ŏ��������� */
    NnString( const char *s ){ set(s); }
    /** �|�C���^ s ���� size �o�C�g�̃o�C�g��ŏ��������� */
    NnString( const char *s , int size ){ set(s,size); }
    /** ������ ns �ŏ��������� */
    NnString( const NnString &ns );

    // ������Z�q.
    /** �|�C���^ s ���� size �o�C�g�̃o�C�g��������� */
    int assign( const char *s , int size );
    /** ������ ns �������� */
    NnString &operator = ( const NnString &ns );
    /** ������ x �������� */
    NnString &operator = ( const char *x );
    /** ���� c �������� */
    NnString &operator = ( char c ){ assign( &c , 1 ); return *this; }

    NnString &insertAt( int at , const char *s , int siz);
    NnString &insertAt( int at , const char *s )
	{ return insertAt(at,s,strlen(s) ); }
    NnString &insertAt( int at , const NnString &ns )
	{ return insertAt(at,ns.chars(),ns.length() ); }

    /** ���� c �𖖔��ɒǉ����� */
    NnString &operator += ( char c );
    /** ������ s �𖖔��ɒǉ����� */
    NnString &operator += ( const char *s )
	{ return insertAt( length(), s, strlen(s) ); }
    /** ������ ns �𖖔��ɒǉ����� */
    NnString &operator += ( const NnString &ns )
	{ return insertAt( length(), ns ); }

    /** ���� c �𖖔��ɒǉ����� */
    NnString &operator << ( char c ){ return *this += c ; }
    /** ������ s �𖖔��ɒǉ����� */
    NnString &operator << ( const char *s ){ return *this += s; }
    /** ������ ns �𖖔��ɒǉ����� */
    NnString &operator << ( const NnString &ns )
	{ return insertAt( length() , ns ); }
    /** ���l�� 10�i������֕ϊ����Ė����ɒǉ����� */
    NnString &addValueOf( int x );
    
    /* �����P���� */
    NnString &operator << (letter_t s)
	{ *this += (char)(s>>8) ; return *this += (char)(s & 255); }

    // ���|�[�g�֐�.
    /** char*�^����������o�� */
    const char *chars()  const { return rep->buffer; }
    /** �󕶎��ł���� �^��Ԃ� */
    int         empty()  const { return rep->length <= 0 ; }
    /** ������̒����𓾂� */
    int         length() const { return rep->length; }
    /** n�o�C�g�ڂ̕����𓾂� */
    char        at(int n) const { return rep->buffer[n]; }
    char        &operator[]( int n ) { return rep->buffer[n]; }
    /** ���������ɂ��� */
    void        erase(){ reset(rep); }

    /** ������ s �Ɣ�r���A�召�֌W�𕉁E��E���̐��l�ŕԂ� */
    virtual int compare( const char *s ) const;
    /** ������ s �Ɣ�r���A�召�֌W�𕉁E��E���̐��l�ŕԂ�
     *  �p�啶���E����������ʂ��Ȃ� */
    int icompare( const char *s ) const;
    /** ������ s �Ɣ�r���A�召�֌W�𕉁E��E���̐��l�ŕԂ�
     *  �p�啶���E����������ʂ��Ȃ� */
    int icompare( const NnString &ns ) const;
    
    /** ������̑O��̋󔒂��폜���� */
    NnString &trim();
    /** �����̂P�o�C�g���폜���� */
    void chop();
    /** ������� len �o�C�g�ɂ��Ă��܂�(�؂�) */
    void chop(int len);
    /** �擪�̂P�o�C�g���폜���� */
    void shift();
    /** �擪�ɂP�o�C�g��}������ */
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

    /** ��������󔒂ŕ������� */
    void splitTo( NnString &first , NnString &rest ) const ;
    int splitTo( NnString &first , NnString &rest , const char *dem , const char *quote="\"" , const char *escape="^" ) const;
    int splitTo( NnVector &vec ) const;

    /** �����̂P����(DBCS�ӎ�)�̏�ʂW�r�b�g��Ԃ� */
    int lastchar() const;

    /** s �Ŏn�܂��Ă���� �^��Ԃ� */
    int   startsWith( const char *s     ) const;
    int   startsWith( const NnString &s ) const;
    /** s �Ŏn�܂��Ă���΁A�^��Ԃ��B�p�啶���E����������ʂ��Ȃ� */
    int  istartsWith( const char *s     ) const;
    int  istartsWith( const NnString &s ) const;

    /** s �ŏI����Ă���΁A�^��Ԃ��B*/
    int     endsWith( const char *s     ) const;
    int     endsWith( const NnString &s ) const;
    /** s �ŏI����Ă���΁A�^��Ԃ��B�p�啶���E����������ʂ��Ȃ��B*/
    int    iendsWith( const char *s     ) const;
    int    iendsWith( const NnString &s ) const;
    /* func �Ŕ�r�������ʁAs �ŏI����Ă���΁A�^��Ԃ� */
#ifdef __DMC__
    int    endsWith( const char *s , int __CLIB func(const char *,const char *) ) const;
#else
    int    endsWith( const char *s , int func(const char *,const char *) ) const;
#endif
    
    /** �n�b�V���l��Ԃ� */
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
    
    /* 1�������ŁA������𑖍�����|�C���^ */
    class Iter{
	letter_t last;
    public:
	const char *next; /* ���̈ʒu���w�肷��|�C���^�͌��J���Ă��܂� */

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

/* NnString �Ɠ��������Acompare/sort/uniq���{�̍ۂɁA�啶���E��������
 * ��ʂ��Ȃ��B*/
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
