#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "nnhash.h"

/* 汎用に使う、プロパティークラス */
NnHash properties;

/* Pair - ハッシュテーブルの要素 : キーと値のペアに関するクラス */
NnHash::Pair::~Pair()
{
    delete value_;
}

inline void NnHash::Pair::replace( NnObject *val )
{
    if( value_ != val )
        delete value_;
    value_ = val;
}
inline NnHash::Pair *NnHash::Pair::connect( NnHash::Pair *ele )
{
    Pair *rv=next_;
    next_ = ele;
    return rv;
}

/* HashTable を初期化する
 * return
 *	0 ... 成功
 *     -1 ... 失敗
 */
int NnHash::init()
{
    if( table == NULL ){
        if( (table=new Pair*[size]) == NULL )
            return -1;
        for(int i=0;i<size;i++)
            table[i] = NULL;
    }
    return 0;
}

/* NnHash::put
 *	key の値として rep を登録する。
 *	key == 0 の時、削除する。
 * return
 *       0 : 成功
 *      -1 : 初期化エラー(テーブル全体の確保の失敗)
 *      -2 : メモリ確保エラー(要素単位)
 */
int NnHash::put_(const NnString &key, NnObject *obj)
{
    if( init() ) return -1;

    int index=key.hashValue() % size;

    if( table[index] == NULL ){
        if( obj != NULL && (table[index]=new Pair(key,obj))==NULL )
            return -2;
        return 0;
    }
    Pair *cur=table[index];
    Pair **pre=&table[index];
    for(;;){
        if( cur->key().compare(key)==0 ){
            /* キーが同じ！ */
            if( obj != NULL ){
                /* 値が非NULLの場合、要素を置換 */
                cur->replace( obj );
            }else{
                /* 値が NULL の場合、要素の削除 */
                *pre = cur->next();
                delete cur;
            }
            return 0;
        }
        if( cur->next() == NULL ){
            /* 新規オブジェクト */
            if( obj != NULL ){
                Pair *t=new Pair(key,obj);
                if( t == NULL )
                    return -2;
                cur->connect( t );
            }
            return 0;
        }
        pre = &cur->next_;
        cur = cur->next();
    }
}

NnObject *NnHash::get(const char *key)
{
    if( table == NULL ) return NULL;
    int index=NnString::hashValue(key) % size;

    for(Pair *ptr=table[index] ; ptr != NULL ; ptr=ptr->next() ){
        if( ptr->key().compare(key)==0 )
            return ptr->value();
    }
    return NULL;
}


/* 連想配列を検索する。
 *	key キー値 
 * return
 *	非NULL … オブジェクトへのポインタ
 *	NULL   … マッチするオブジェクトは無かった。
 */
NnObject *NnHash::get(const NnString &key)
{
    if( table == NULL ) return NULL;
    int index=key.hashValue() % size;
  
    for(Pair *ptr=table[index] ; ptr != NULL ; ptr=ptr->next() ){
        if( ptr->key().compare(key)==0 )
            return ptr->value();
    }
    return NULL;
}

/* デストラクタ
 */
NnHash::~NnHash()
{
    if( table == NULL )
	return;

    for(int i=0;i<size;i++){
	Pair *p=table[i];
	while( p != NULL ){
	    Pair *nxt=p->next();
            delete p;
	    p = nxt;
	}
    }
    delete[]table;
}

/* ハッシュ用カーソル初期化
 *	hash - ハッシュ
 */
NnHash::Each::Each(const NnHash &h) : hash(h)
{
    if( hash.table != NULL ){
        index=-1;
        nextindex();
    }else{
        index = 0;
        ptr = NULL;
    }
}

/* 内部配列に対するインデックスを一つ進める.
 * 空のインデックスは読み飛ばす.
 */
void NnHash::Each::nextindex()
{
    while( ++index < hash.size ){
        if( hash.table[index] != NULL ){
            ptr  = hash.table[index];
            return;
        }
    }
    ptr = NULL;
    index = 0;
}

/* ハッシュ用カーソルを一つ進める. */
void NnHash::Each::operator ++ ()
{
    if( ptr == NULL )
        return;

    if( ptr->next() != NULL ){
        ptr = ptr->next();
        return;
    }
    nextindex();
}

NnObject *NnHash::Each::operator *()
{
    return ptr;
}

NnHash::NnHash( const NnHash &o )
{
    table = 0; size = o.size;
    for( NnHash::Each p(o) ; p.more() ; ++p ){
	this->put( p->key() , p->value()->clone() );
    }
}

/* 環境変数の値を得る(環境変数の大文字/小文字を区別しない)
 *    var - 環境変数名
 *    none - 環境変数が存在しない時に返す値.
 * return
 *     環境変数の値
 */
const char *getEnv( const char *var , const char *none )
{
    if( var == NULL || var[0] == '\0' )
	return NULL;
    const char *value=getenv(var);
    if( value != NULL  && value[0] != '\0' )
	return value;

    NnString var1(var);
    var1.upcase();
    
    return (value=getenv(var1.chars())) != NULL  &&  value[0] != '\0'
            ? value : none ;
}
