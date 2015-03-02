/* NnHash :
 *   文字列(NnString,char*)だけをキーとできるハッシュテーブル。
 *   文字列に関連付けて登録するオブジェクトは NnObject の
 *   派生クラスでなければならない。
 */
#ifndef NNHASH_H
#define NNHASH_H

#include "nnstring.h"

/** ハッシュテーブルのオブジェクト */
class NnHash : public NnObject {
public:
    class Each;
    /** キーと値のペアを保持するオブジェクト  */
    class Pair : public NnObject {
	Pair *next_;
	NnString key_;
	NnObject *value_;

	void replace( NnObject * );
	Pair *connect( Pair * );
	Pair *next() { return next_; }

	Pair( const NnString &k , NnObject *obj=0 )
	    : next_(0) , key_(k) , value_(obj){}
    public:
	~Pair();

	/** キーを返す */
	NnString &key()  { return key_;   }
	/** キーに関連つけられたオブジェクトを返す */
	NnObject *value(){ return value_; }

	friend class Each;
	friend class NnHash;
    };
    /* イタレータ */
    class Each : public ::NnEnum {
	const NnHash &hash;
	Pair  *ptr;
	int index;
	void nextindex();
    public:
	/** コンストラクタ：列挙されるハッシュテーブルを指定する */
	Each( const NnHash &hash );

	/** 現在差している、キーとオブジェクトのペア(Pair)への
	 *  ポインタを返す。戻り値の型が NnObject* なのは、
	 *  Superクラスの NnEnum と型を合わせるため。
	 *  さすべきペアがない場合は NULL が返る。
	 */
	virtual   NnObject *operator *();

	/** 次の要素へ移動する。もはや要素がない場合は、
	 *  operator * にて NULL が返る。
	 */
	virtual   void      operator++();

	/** 現在差している、要素へアクセスする
	 *  キーの場合：        (*this)->key()    (NnString&型)
	 *  オブジェクトの場合：(*this)->value()  (NnObject*型)
	 */
	Pair *operator->(){ return ptr; }
	friend class NnHash;
    };
private:
    Pair **table;
    int size;
    int init();
public:
    int put_(const NnString &key, NnObject *obj);
    /** key に関連つけられたオブジェクトへのポインタを返す。
     *  存在しなければ、NULL を返す。
     */
    NnObject *get(const NnString &key);
    NnObject *get(const char *key);

    /** key に関連つけて、obj を登録する。
     *  obj の delete権は NnHash 内に移動する。
     */
    int  put(const NnString &key, NnObject *obj)
	{ return obj ? put_(key,obj) : -3 ; }

    /** key に関連付けられたオブジェクトを削除する。
     *  オブジェクトは delete される
     */
    void remove(const NnString &key)
	{ put_(key,0); }

    /** コンストラクタ */
    NnHash(int s=256) : table(0) , size(s) { }
    NnHash( const NnHash & );
    ~NnHash();

    NnObject *clone() const { return new NnHash(*this); }
    friend class Each;

#if 0
    class Each {
	char *p;
    public:
	Each( const char *p_ ) : p(p_){}
	int more() const { return *p != '\0'; }
	void next(){ if( isKanji(*p) ){ ++p; } ++p; }
    };
#endif
};

extern NnHash properties;

#endif
