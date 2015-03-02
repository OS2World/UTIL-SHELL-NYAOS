#include <ctype.h>
#include <assert.h>
#include <string.h>
#include "getline.h"

enum{
    UNIT_BUFSIZE = 40 ,
    INIT_BUFSIZE = 160,
};

/* ���[���[�`�� */
int TwinBuffer::makeroom(int at,int size)
{
    /* �o�b�t�@�T�C�Y���������悤�ł���΁A�L���� */
    if( len+size > max ){
        max = len+size+UNIT_BUFSIZE;
        char *tmp = (char*)realloc( strbuf, max+1 );
        if( tmp == NULL )
            return -1;
        strbuf = tmp;
        tmp = (char*)realloc( atrbuf, max+1 );
        if( tmp == NULL )
            return -1;
        atrbuf = tmp;
    }
    for(int i=len-1 ; i >= at ; i-- ){
        strbuf[ i+size ] = strbuf[ i ];
        atrbuf[ i+size ] = atrbuf[ i ];
    }
    len += size;
    strbuf[ len ] = '\0';
    return 0;
}

void TwinBuffer::delroom(int at,int size)
{
    for(int i=at;i<len;i++){
        strbuf[ i ] = strbuf[ i+size ];
        atrbuf[ i ] = atrbuf[ i+size ];
    }
    len -= size;
    strbuf[ len ] = '\0';
}

/* �P�����}��
 *      key - ����(>512:�{�p�����Ƃ݂Ȃ�)
 *      pos - �}�����ʒu
 * return
 *      �}���o�C�g��(0�`2) : 0 �̎��̓G���[
 */
int TwinBuffer::insert1(int key,int pos)
{
    int upperbyte=(key >> 8) & 255;

    if( upperbyte == 1 ){
        return 0;
    }else if( isCtrl(key) ){
	if( makeroom(pos,2) == -1 )
	    return 0;
	strbuf[ pos   ] = '^';
	atrbuf[ pos   ] = DBC1ST;

	strbuf[ pos+1 ] = '@'+key;
	atrbuf[ pos+1 ] = DBC2ND;

	return 2;
    }else if( upperbyte != 0 ){
        /* DBCS���� */
        if( makeroom(pos,2) == -1 )
            return 0;
        strbuf[ pos   ] = upperbyte;
        atrbuf[ pos   ] = DBC1ST;
        
        strbuf[ pos+1 ] = key;
        atrbuf[ pos+1 ] = DBC2ND;

        return 2;
    }else{
        if( makeroom(pos,1) == -1 )
            return 0;
        strbuf[ pos ] = key;
        atrbuf[ pos ] = SBC;

        return 1;
    }
}

/* �P�����폜���s���B
 *      at - �폜���錅�ʒu
 * return
 *      �폜�o�C�g��
 */
int TwinBuffer::erase1(int at)
{
    if( at >= len )
        return 0;
    int siz=sizeAt(at);
    delroom(at,siz);
    return siz;
}

/* at ���ڈȍ~���폜����B
 *      at - �폜�J�n���ʒu
 */
void TwinBuffer::erase_line(int at)
{
    assert( at <= max );
    strbuf[len=at] = '\0';
}

/* ������ x ���ATwinBuffer �Ɋi�[����ۂɕK�v�ɂȂ�T�C�Y(����)���Z�o����B
 * �ʏ�A���䕶����2���Ɛ������鑼�� byte �Ɠ����B
 */
int TwinBuffer::strlen_ctrl(const char *x)
{
    int len=0;
    while( *x != '\0'){
	if( isCtrl(*x) )
	    len++;
	len++;
	++x;
    }
    return len;
}

/* ������ s �� at ���ڂɑ}������B
 *      s - �}��������
 *      at - �}���ʒu
 * return 
 *      �}�������o�C�g��
 */
int TwinBuffer::insert(const char *s,int at)
{
    if( s == NULL )
        return 0;

    int len=strlen_ctrl(s);
    if( makeroom( at , len ) == -1 )
        return 0;

    int afterkanji=0;

    while( *s != '\0' ){
        if( afterkanji ){
            atrbuf[ at ] = DBC2ND;
            afterkanji = 0;
	    strbuf[ at++ ] = *s++;
        }else if( isKanji(*s) ){
            atrbuf[ at ] = DBC1ST;
            afterkanji = 1;
	    strbuf[ at++ ] = *s++;
        }else if( isCtrl(*s) ){
	    atrbuf[ at   ] = DBC1ST;
	    strbuf[ at++ ] = '^';
	    atrbuf[ at   ] = DBC2ND;
	    strbuf[ at++ ] = '@'+*s++;
	    afterkanji = 0;
	}else{
            atrbuf[ at ] = SBC;
            afterkanji = 0;
	    /* ���s�̗ނ͋󔒂ɒ����Ă��܂� */
	    if( *s == '\n' || *s == '\r' ){
		strbuf[ at++ ] = ' ';
		++s;
	    }else{
		strbuf[ at++ ] = *s++;
	    }
	}
    }
    return len;
}

/* ������ 
 * return 0�c���� , -1�c���s
 */
int TwinBuffer::init()
{
    strbuf = (char*)malloc( INIT_BUFSIZE+1 );
    if( strbuf == NULL )
        return -1;

    atrbuf = (char*)malloc( INIT_BUFSIZE+1 );
    if( atrbuf == NULL ){
        free(strbuf);
        strbuf = NULL;
        return -1;
    }
    max = INIT_BUFSIZE;
    len = 0 ;
    strbuf[ len ] = '\0';
    return 0;
}

void TwinBuffer::term()
{
    free(strbuf);strbuf=NULL;
    free(atrbuf);atrbuf=NULL;
    max = len = 0;
}


/* at ���� nchars �o�C�g���� string �Œu������
 * return
 *      �u�������邱�Ƃɂ�鑝���l�B
 *      �G���[���FEXCEPTIONS(-32768)
 */
int TwinBuffer::replace(int at,int nchars, const char *string )
{
    int new_nchars=strlen_ctrl(string);

    if( nchars < new_nchars ){
        if( makeroom( at+nchars , new_nchars-nchars ) == -1 )
            return EXCEPTIONS;
    }else if( nchars > new_nchars ){
        delroom( at+new_nchars , nchars-new_nchars);
    }

    int j=0,i=0;
    while( i < new_nchars ){
        if( isKanji( string[i] ) && i<new_nchars-1 ){
            strbuf[ at+j   ] = string[i++];
            atrbuf[ at+j++ ] = DBC1ST;
            strbuf[ at+j   ] = string[i++];
            atrbuf[ at+j++ ] = DBC2ND;
	}else if( isCtrl( string[i] ) ){
	    strbuf[ at+j   ] = '^';
	    atrbuf[ at+j++ ] = DBC1ST;
	    strbuf[ at+j   ] = '@' + string[i++];
	    atrbuf[ at+j++ ] = DBC2ND;
        }else{
            strbuf[ at+j   ] = string[i++];
            atrbuf[ at+j++ ] = SBC;
        }
    }
    return j-nchars;
}

/* ^x �`���� Ctrl �R�[�h���f�R�[�h����.
 *	at - �ǂݏo���J�n�ʒu
 *	len - �ǂݏo���T�C�Y
 *	buffer - �i�[��
 * return
 *	�f�R�[�h��̃T�C�Y.
 */
int TwinBuffer::decode(int at , int len , NnString &buffer )
{
    buffer.erase();
    for(int i=0;i<len;i++){
	if( strbuf[at+i]=='^' &&  atrbuf[at+i]==DBC1ST ){
	    buffer << (char)(strbuf[at+ ++i] & 0x1F);
	}else{
	    buffer << (char)strbuf[at+i];
	}
    }
    return buffer.length();
}
int TwinBuffer::decode(NnString &buffer)
{
    return decode(0,this->length(),buffer);
}
