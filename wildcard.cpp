#include <assert.h>
#include <stddef.h>
#include "nnstring.h"
#include "nnvector.h"
#include "nndir.h"

/* ディレクトリ展開（ソートなし）
 *      path - パス
 *      list - 展開結果
 * return
 *       0 - 成功(パスにワイルドカードが含まれなかった時も含む)
 *      -1 - メモリアローケーションエラー.
 */
int fnexplode_(const char *path , NnVector &list )
{
    int lastpos=NnDir::lastRoot(path);
    int dotprint=0;
    NnString dirname;

    /* ワイルドカード文字が含まれていなければ終了 */
    if( NnString::findOf(path,"*?") == -1 )
	return 0;

    if( lastpos >= 0 ){
	switch( path[lastpos+1] ){
	case '\0': // 非ディレクトリ部分が存在しない(末尾が / \ : のいずれか)
	    {
		NnString p( path , lastpos );
		return fnexplode_( p.chars() , list );
	    }
	case '.':  // 非ディレクトリ部分が . で始まる.
	    dotprint = 1;
	    break;
	}
        /* 元パス名にはディレクトリが含まれている */
        dirname.assign( path , lastpos+1 );
    }else{
        /* 元パス名にはディレクトリは含まれていない */
        if( path[0] == '.' )
            dotprint = 1;
    }

    for( NnDir dir(path) ; dir.more() ; dir.next() ){
        // 「.」で始まるファイルは基本的に表示しない。
        if(  dir->at(0) == '.' ){
            // ファイル名自体の指定で「.」で始まる場合ば別
            if( dotprint==0 )
                continue;
            // 「.」自体は展開しない
            if( dir->at(1)=='\0' )
                continue;
        }

	NnString path1( dirname );
	path1 << dir.name();

	list.append( 
	    new NnFileStat( path1, dir.attr() , dir.size() , dir.stamp() ) 
	);
    }
    return 0;
}

/* ディレクトリ展開（ソートあり）
 *      path - パス
 *      list - 展開結果
 * return
 *       0 - 成功(パスにワイルドカードが含まれなかった時も含む)
 *      -1 - メモリアローケーションエラー.
 */
int fnexplode( const char *path , NnVector &list )
{
    int rc = fnexplode_( path , list );
    if( rc == 0 ){
        list.sort();
        list.uniq();
    }
    return rc;
}

NnString &glob_all( const char *source_ , NnString &result )
{
    NnString left(source_) , frag ;
    while( left.splitTo( frag , left ) , !frag.empty() ){
        NnVector temp;
        NnString frag_ = frag; frag_.dequote();

        if( fnexplode( frag_.chars() , temp ) == 0 && temp.size() > 0 ){
            for(int i=0;i<temp.size();++i){
                NnFileStat *file=dynamic_cast<NnFileStat*>( temp.at(i) );
                assert( file != NULL );
                const NnString &fn=file->name();

                if( fn.findOf(" ") >= 0 ){
                    result << '"' << fn << "\" ";
                }else{
                    result << fn << ' ';
                }
            }
        }else{
            result << frag << ' ';
        }
    }
    result.chop();
    return result;
}
