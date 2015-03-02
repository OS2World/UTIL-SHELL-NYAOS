#include <assert.h>
#include <stddef.h>
#include "nnstring.h"
#include "nnvector.h"
#include "nndir.h"

/* �f�B���N�g���W�J�i�\�[�g�Ȃ��j
 *      path - �p�X
 *      list - �W�J����
 * return
 *       0 - ����(�p�X�Ƀ��C���h�J�[�h���܂܂�Ȃ����������܂�)
 *      -1 - �������A���[�P�[�V�����G���[.
 */
int fnexplode_(const char *path , NnVector &list )
{
    int lastpos=NnDir::lastRoot(path);
    int dotprint=0;
    NnString dirname;

    /* ���C���h�J�[�h�������܂܂�Ă��Ȃ���ΏI�� */
    if( NnString::findOf(path,"*?") == -1 )
	return 0;

    if( lastpos >= 0 ){
	switch( path[lastpos+1] ){
	case '\0': // ��f�B���N�g�����������݂��Ȃ�(������ / \ : �̂����ꂩ)
	    {
		NnString p( path , lastpos );
		return fnexplode_( p.chars() , list );
	    }
	case '.':  // ��f�B���N�g�������� . �Ŏn�܂�.
	    dotprint = 1;
	    break;
	}
        /* ���p�X���ɂ̓f�B���N�g�����܂܂�Ă��� */
        dirname.assign( path , lastpos+1 );
    }else{
        /* ���p�X���ɂ̓f�B���N�g���͊܂܂�Ă��Ȃ� */
        if( path[0] == '.' )
            dotprint = 1;
    }

    for( NnDir dir(path) ; dir.more() ; dir.next() ){
        // �u.�v�Ŏn�܂�t�@�C���͊�{�I�ɕ\�����Ȃ��B
        if(  dir->at(0) == '.' ){
            // �t�@�C�������̂̎w��Łu.�v�Ŏn�܂�ꍇ�Ε�
            if( dotprint==0 )
                continue;
            // �u.�v���͓̂W�J���Ȃ�
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

/* �f�B���N�g���W�J�i�\�[�g����j
 *      path - �p�X
 *      list - �W�J����
 * return
 *       0 - ����(�p�X�Ƀ��C���h�J�[�h���܂܂�Ȃ����������܂�)
 *      -1 - �������A���[�P�[�V�����G���[.
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
