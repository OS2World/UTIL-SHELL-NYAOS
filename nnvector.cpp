#include <stdlib.h>
#include "nnstring.h"
#include "NnVector.h"

/* 配列を拡張する。
 *	n : 新サイズ
 * return
 *	0 ... 成功 , -1 ... 失敗
 */
int NnVector::grow(int n)
{
    // if( n <= max_ ) return 0;
    NnObject **newVector = (NnObject**)(array != NULL 
			? realloc( array , n*sizeof(NnObject*) )
			: malloc( n*sizeof(NnObject*) ) );
    if( newVector == NULL )
        return -1;
    
    array = newVector;
    while( max_ < n )
        array[ max_++ ] = NULL;
    return 0;
}

/* デストラクタ */
NnVector::~NnVector()
{
    if( array ){
	for(int i=0 ; i<size() ;i++ )
            delete array[i];
	free( array );
    }
}

/* 要素を末尾に追加する.
 *      obj - 追加するオブジェクト
 * return
 *	0 ... 成功 
 *     -1 ... メモリ確保失敗
 *     -2 ... パラメータエラー(obj が NULL)
 */
int NnVector::append( NnObject *obj )
{
    if( obj == NULL )
        return -2;
    
    while( size_+1 >= max_ ){
	if( grow(max_+10) != 0 )
	    return -1;
    }
    array[ size_++ ] = obj;
    return 0;
}

/* qsort x NnSortable* 向け比較関数.
 *      x 要素1(NnObject*へのポインタ)
 *      y 要素2(NnObject*へのポインタ)
 * return
 *      大小関係を表す整数値(x-yに相当)
 */
static int 
compare(const void *x,const void *y)
{
    NnSortable *xs=( *(NnObject**)x )->sortable();
    NnSortable *ys=( *(NnObject**)y )->sortable();
    if( xs == NULL ) 
	return ys != NULL ? -1 : 0;
    if( ys == NULL )
	return +1;
    
    return xs->compare(*ys);
}

/* ソートを行う.
 * ・NnSortable のインスタンスでないものは、
 * ・配列の全要素が NnSortable* であることが使用条件。
 */
void NnVector::sort()
{
    if( array != NULL  &&  this->size() >= 2 )
        qsort( array , this->size() , sizeof(NnObject*) , &compare );
}

/* ソートを行う。比較は引数の関数オブジェクトで行う.
 * (アルゴリズムは selection sort o(n^2) :簡単だが、余り速くない)
 */
void NnVector::sort( NnComparer &comparer )
{
    if( array == NULL ||  this->size() < 2 )
	return;

    for(int i=0 ; i<this->size() ; i++ ){
	for(int j=i+1 ; j<this->size() ; j++ ){
	    if( array[i] == NULL || comparer( array[i] , array[j] ) > 0 ){
		NnObject *tmp=array[i];
		array[i] = array[j];
		array[j] = tmp;
	    }
	}
    }
}

/* at番目の要素を除き、後をつめる。
 * 実行後、size() は 1 減る。
 *      at - 取り除く位置
 * return
 *      除いたオブジェクト(deleteはしていない)
 */
NnObject *NnVector::removeAt( int at )
{
    if( at >= size() )
        return NULL;
    NnObject *rc=array[at];
    while( at+1 < size() ){
        array[at] = array[at+1];
        ++at;
    }
    array[--size_] = NULL;
    return rc;
}

/* 重複オブジェクトを削除する。
 * 下記の条件を満たす必要がある。
 * ・配列の全要素が NULL か NnSortable* である。
 * ・事前に sort() を実施している。
 */
void NnVector::uniq()
{
    if( array==NULL || this->size() < 2 )
        return;
    
    while( array[0] == NULL && size() > 1 )
        deleteAt(0);
    
    NnSortable *lhs=array[0]->sortable();
    if( lhs == NULL )
        return;
        
    for(int p=0 ; (p+1)<size() ; p++ ){
        NnSortable *rhs=NULL;
        while(  p+1 < size() 
            && (   array[p+1] == NULL 
                || (rhs=array[p+1]->sortable())== NULL
                || lhs->compare(*rhs)==0 )
        ){
            deleteAt(p+1);
        }
        lhs = rhs;
    }
}
void NnVector::erase()
{
    for(int i=0;i<size_;++i){
	delete array[i];
	array[i] = 0;
    }
    size_ = 0;
}

NnObject *NnVector::clone() const
{
    NnVector *result=new NnVector();
    for(int i=0;i<size();i++){
	result->append( const_at(i)->clone() );
    }
    return result;
}

/* NnString のメソッドだが、NnVector が利用される時しか、
 * 使われないので、こちらのソースに入れておく */

/* 文字列を空白で分割して、配列に格納する.
 *    param 格納先
 * return
 *    分割数
 */
int NnString::splitTo( NnVector &param ) const
{
    int n=0;
    NnString left( *this );
    while( ! left.empty() ){
	NnString *arg1=new NnString();
	left.splitTo(*arg1,left);
	param.append( arg1 );
	++n;
    }
    return n;
}
