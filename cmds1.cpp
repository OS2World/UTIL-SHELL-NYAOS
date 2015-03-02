#include <stddef.h>
#include <stdlib.h>
#include <ctype.h>
#ifndef __EMX__
#  include <dir.h>
#endif
#include <errno.h>

#include "getline.h"
#include "shell.h"
#include "nnhash.h"
#include "nndir.h"

extern NnHash properties;
extern NnHash aliases;

/* �G�C���A�X�Ȃǂ̐ݒ�n�̃R�}���h�̋��ʏ���
 *      hash - �����n�b�V���e�[�u��
 *      argv - �ݒ���e
 *              �� => �S�Ă̑Ή���W���o�͂֏o��
 *              �P�g�[�N���̂� => ���̃g�[�N���ɑΉ�����l���o��
 *              �Q�g�[�N���ȏ� => �ŏ��̃g�[�N���Ɍ�̓��e��������
 *      lower - 1 �̂Ƃ��A�l�� tolower �̃t�B���^��������B
 * return
 *      ��� 0
 */
int cmd_setter( NyadosShell &shell , NnHash &hash , const NnString &argv )
{
    if( argv.empty() ){
	// �ꗗ�\��.
	for( NnHash::Each itr(hash) ; itr.more() ; ++itr )
            conOut 
                << itr->key() << ' '
                << *(NnString*)itr->value()
                << '\n';
	return 0;
    }
    NnString name;
    NnString *value=new NnString;
    if( value == NULL )
        return 0;
    argv.splitTo( name , *value );
    int equalPos=name.findOf("=");
    if( equalPos >= 0 ){ /* bash �݊��̐ݒ莮 */
        NnString left ( name.chars() , equalPos );
        NnString right( name.chars()+equalPos+1 );
        name = left;
        right << ' ' << *value;
        *value = right;
    }
    name.downcase();
    value->trim();

    if( name.length() <= 0 ){
	delete value;
    }else if( value->length() <= 0 ){
	if( name.at(0)=='+'){
	    name.shift();
	    hash.put( name , new NnString() );
	}else if( name.at(0)=='-'){
	    name.shift();
	    hash.remove( name );
	}else{
	    // �P�G�C���A�X���e�\��.
	    NnString *rvalue=(NnString *)hash.get( name );
	    if( rvalue != NULL )
                conOut << name << ' ' << *rvalue << '\n';
	    delete value;
	}
    }else{
	// �G�C���A�X��`.
	if( value->startsWith("\"") ){
	    NnString tmp;

	    NyadosShell::dequote( value->chars() , tmp );
	    *value = tmp;
	}
	hash.put( name , value );
    }
    return 0;
}


/* �G�C���A�X�̒�`�Q�Ƃ��s��.
 *	argv - ����
 * return
 *	��� 0
 */
int cmd_alias( NyadosShell &shell , const NnString &argv )
{
    return cmd_setter( shell , aliases , argv );
}

/* �ʖ����폜���� */
int cmd_unalias( NyadosShell & , const NnString &argv )
{
    aliases.remove( argv );
    return 0;
}

int cmd_suffix( NyadosShell &shell , const NnString &argv )
{
    return cmd_setter( shell , DosShell::executableSuffix , argv );
}

int cmd_unsuffix( NyadosShell & , const NnString &argv )
{
    NnString argvL(argv);
    argvL.downcase();
    DosShell::executableSuffix.remove( argvL );
    return 0;
}

int cmd_history( NyadosShell &shell , const NnString &argv )
{
    int n=10;
    if( ! argv.empty()  &&  isDigit(argv.at(0)) ){
        n = atoi( argv.chars() );
        if( n <= 0 )
            n = 10;
    }
    History *hisObj=shell.getHistoryObject();
    if( hisObj == NULL )
	return 0;
    int start=hisObj->size()-n;
    if( start < 0 )
        start = 0;
    
    for(int i=start ; i<hisObj->size() ; i++ ){
        History1 history;
	hisObj->get(i,history);
        NnString line( history.body() );

        conOut << i << ' ' << history.stamp() << ' ';
	for(int j=0;j< line.length();++j){
	    if( TwinBuffer::isCtrl( line.at(j)) )
                conOut << '^' << (char)('@'+line.at(j));
	    else
                conOut << (char)line.at(j);
	}
        conOut << '\n';
    }
    return 0;
}

int cmd_option( NyadosShell &shell , const NnString &argv )
{
    return cmd_setter( shell , properties , argv );
}
int cmd_folder( NyadosShell &shell , const NnString &argv )
{
    return cmd_setter( shell , NnDir::specialFolder , argv );
}

int cmd_unoption( NyadosShell & , const NnString &argv )
{
    properties.remove( argv );
    return 0;
}
