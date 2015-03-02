#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>
#include "nndir.h"
#include "nnstring.h"
#include "shell.h"

/* ディレクトリではなく、ファイルとして存在していれば 1
 * さもなければ 0 を返す
 */
static int is_file( const NnString &path )
{
    NnFileStat *st = NnFileStat::stat( path );
    if( st == NULL ){
        return 0;
    }else if( st->isDir() ){
        delete st;
        return 0;
    }else{
        delete st;
        return 1;
    }
}


/* フルパス一歩手前になっているファイル名に、.exe 拡張子を補って、
 * 存在があるかを確認する
 *    nm - フルパス一歩手前になっているファイル名
 *    which - 見付かった時に、補ったパス名を格納する先
 * return
 *     0 - 見付かった(which に値が入る)
 *    -1 - 見付からなかった
 */
static int exists( const char *nm , NnString &which )
{
    static const char *suffix_list[]={
        ".exe" , ".com" , ".bat" , ".cmd" , NULL 
    };
    NnString path(nm);

    for( const char **p=suffix_list ; *p != NULL ; ++p ){
        path << *p;
        if( is_file(path) ){
            which = path ;
            return 0;
        }
        path.chop( path.length()-4 );
    }
    if( is_file(path) ){
        which = path ;
        return 0;
    }
    return -1;
}


/* 実行ファイルのパスを探す
 *      nm      < 実行ファイルの名前
 *      which   > 見つかった場所
 * return
 *      0 - 発見
 *      -1 - 見つからず
 */
int which( const char *nm, NnString &which )
{
    if( exists(nm,which)==0 )
        return 0;

    /* 相対パス指定・絶対パス指定しているものは、
     * 環境変数PATHをたどってまで、検索しない
     */
    if( NnString::findLastOf(nm,"/\\") >= 0 ){
        return -1;
    }

    NnString rest(".");
    const char *env=getEnv("PATH",NULL);
    if( env != NULL )
	rest << ";" << env ;
    
    while( ! rest.empty() ){
	NnString path;
	rest.splitTo( path , rest , ";" );
	if( path.empty() )
	    continue;
        path << '\\' << nm;
        if( exists(path.chars(),which)==0 )
            return 0;
    }
    return -1;
}
