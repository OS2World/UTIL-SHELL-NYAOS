#ifndef NNVECTOR_H
#define NNVECTOR_H

#include "nnobject.h"

/* ソートを行うための比較を行う関数オブジェクト
 */
class NnComparer : public NnObject {
public:
    virtual int operator()( const NnObject *left ,
			    const NnObject *right )=0;
};

/** 可変長サイズの配列。要素として含められるのは、
 *  NnObject の派生クラスのみ。
 */
class NnVector : public NnObject {
    NnObject **array;
    int max_,size_;
    int grow(int n);
public:
    NnVector()      : array(0) , max_(0) , size_(0){}
    NnVector(int n) : array(0) , max_(0) , size_(0){ grow(n); }
    ~NnVector();

    /** 要素の個数を返す */
    int size() const { return size_; }

    /** i 番目の要素を返す。要素は delete 禁止 */
    NnObject *at(int i){ return array[i]; }

    /** i 番目の要素を返す。要素は変更禁止 */
    const NnObject *const_at(int i) const { return array[i]; }

    /** オブジェクトを末尾へ追加する。
     *  オブジェクトの delete 権は NnVector へ移動する。*/
    int append( NnObject *obj );

    /** 最後の要素を削除する。
     * オブジェクトは delete せず、戻り値として返す。
     *  (delete権がユーザへ移動する)
     */
    NnObject *pop(){ 
	if( size_ <= 0 ) return 0;
	NnObject *rc=array[--size_];
	array[size_]=0;
	return rc;
    }

    /** 最初の要素を、戻り値として返す。*/
    NnObject *top(){ return size_ >0 ? array[size_-1] : 0 ; }

    /** x 番目の要素を削除し、後を詰める。
     *  オブジェクトは delete せず、戻り値として返す。
     *  (delete権がユーザへ移動する)
     */
    NnObject *removeAt(int i);

    /** 最初の要素を削除し、後を詰める。
     *  オブジェクトは delete せず、戻り値として返す。
     *  (delete権がユーザへ移動する)
     */
    NnObject *shift(){ return removeAt(0); }

    /** at 番目の要素を delete し、後を詰める */
    void deleteAt(int i){ delete removeAt(i); }

    /** 含まれる要素をソートする。大小関係のない要素は、末尾へ移動される */
    void sort();
    void sort( NnComparer &comparer );

    /** 連続する同一要素の一方を delete し、詰める */
    void uniq();

    void erase();
    virtual NnObject *clone() const;
};

#endif
