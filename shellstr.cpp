#include <stdlib.h>
#include "nnstring.h"
#include "shell.h"

/** buf �̒��ɋ󔒂�����΁A�O�������p���ň͂ށB
 *	buf - ��������
 *	dst - �ϊ��㕶����
 */
void NyadosShell::enquote( const char *buf , NnString &dst )
{
    if( strchr(buf,' ') != NULL ){
	dst << '"' << buf << '"';
    }else{
	dst << buf ;
    }
}

/** ������ p ����A���p���������B�������A"" �� " �ɕϊ�����B
 *	p - ��������
 *	result - �ϊ��㕶����
 */
void NyadosShell::dequote( const char *p , NnString &result )
{
    result.erase();
    int quotecount=0;
    for( ; *p != '\0' ; ++p ){
	if( *p == '"' ){
	    if( ++quotecount % 2 == 0 )
		result << '"';
	}else if( *p == '\n' ){
	    result << "$T";
	    quotecount = 0;
	}else{
	    result << (char)*p;
	    quotecount = 0;
	}
    }
}

void NyadosShell::dequote( NnString &result )
{
    NnString result_;

    dequote( result.chars() , result_ );
    result = result_ ;
}


/* �������ǂݎ��B
 * ���p���Ȃǂ��F������B
 *      sp - ������擪�|�C���^
 *            �����s��́A�����̎��̕����ʒu(��,\0 �ʒu��)
 *      token - ������������
 * return
 *      0 - ����
 *      '<', '>','|'
 *      EOF - ����
 */
int NyadosShell::readWord( const char *&sp , NnString &token )
{
    int quote=0;
    token.erase();
    for(;;){
        if( *sp == '\0' )
            return EOF;
        if( isSpace(*sp) && quote==0 )
            return 0;
        if( quote==0  &&  (*sp=='<' || *sp=='>' || *sp=='|' ) )
            return *sp;

        if( quote==0  &&  *sp=='%' ){
            NnString envname;
            for(;;){
                ++sp;
                if( *sp == '\0' ){
                    token << '%' << envname;
                    return EOF;
                }
                if( *sp == '<' || *sp == '>' || *sp == '|' ){
                    token << '%' << envname;
                    return *sp;
                }
                if( *sp == '"' ){
                    quote = !quote;
                    token << '%' << envname;
                    break;
                }
                if( *sp == '%' ){
                    const char *envval=getEnv( envname.chars() , NULL );
                    if( envval == NULL ){
                        token << '%' << envname << '%';
                    }else{
                        token << envval;
                    }
                    break;
                }
                envname << *sp;
            }
        }else{
	    if( *sp == '"' )
		quote = !quote;
	    else
		token << *sp;
        }
        ++sp;
    }
}
/* �X�y�[�X�������X�L�b�v����
 *	sp : �ϊ��O��̕�����
 * return
 *	0 : ����
 *	-1 : �X�y�[�X�ȊO�̕�����������Ȃ������B
 */
int NyadosShell::skipSpc( const char *&sp )
{
    for(;;){
        if( *sp == '\0' )
            return -1;
        if( !isSpace(*sp) )
            return 0;
        ++sp;
    }
}

/* ���_�C���N�g���ǂݎ��B
 *      sp - '>' �Ȃǂ̕����̎����w���Ă���|�C���^
 *           �� ���s��͖����̎��̕����ʒu��(��,\0)
 *      fn - �t�@�C����
 */
void NyadosShell::readNextWord( const char *&sp , NnString &fn )
{
    if( skipSpc( sp ) == -1 ){
	fn.erase();
	return ;
    }
    readWord( sp , fn );
    fn.slash2yen();
}
