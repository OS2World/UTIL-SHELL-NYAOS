#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>
#include "nndir.h"
#include "nnstring.h"
#include "shell.h"

/* �f�B���N�g���ł͂Ȃ��A�t�@�C���Ƃ��đ��݂��Ă���� 1
 * �����Ȃ���� 0 ��Ԃ�
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


/* �t���p�X�����O�ɂȂ��Ă���t�@�C�����ɁA.exe �g���q�����āA
 * ���݂����邩���m�F����
 *    nm - �t���p�X�����O�ɂȂ��Ă���t�@�C����
 *    which - ���t���������ɁA������p�X�����i�[�����
 * return
 *     0 - ���t������(which �ɒl������)
 *    -1 - ���t����Ȃ�����
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


/* ���s�t�@�C���̃p�X��T��
 *      nm      < ���s�t�@�C���̖��O
 *      which   > ���������ꏊ
 * return
 *      0 - ����
 *      -1 - �����炸
 */
int which( const char *nm, NnString &which )
{
    if( exists(nm,which)==0 )
        return 0;

    /* ���΃p�X�w��E��΃p�X�w�肵�Ă�����̂́A
     * ���ϐ�PATH�����ǂ��Ă܂ŁA�������Ȃ�
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
