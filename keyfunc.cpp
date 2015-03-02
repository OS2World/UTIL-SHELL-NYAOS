/* ��s���̓��W���[�� Getline �ł́A
 * �L�[�Ƀo�C���h�����R�}���h�̓�����L�q
 */

#include <ctype.h>
#include <stdio.h>
#include "getline.h"
#include "ntcons.h"

#ifdef NYACUS
#  include <windows.h>
#endif

/* �R�}���h�FCTRL-A�F�J�[�\����擪�ֈړ�������B*/
Status GetLine::goto_head(int)
{
    putbs( pos-offset );
    pos = offset = 0;
    repaint_after();
    return NEXTCHAR;
}

void GetLine::backward_( int n )
{
    pos -= n;
    if( pos < offset ){
        offset -= n;
        repaint_after();
    }else{
        putbs(n);
    }
}

/* �R�}���h�FCTRL-B�F�J�[�\������O�Ɉړ�������B*/
Status GetLine::backward(int)
{
    if( pos > 0 )
	backward_( buffer.preSizeAt(pos) );
    return NEXTCHAR;
}

Status GetLine::backward_word(int)
{
    int top=0;
    int i=0;
    for(;;){
	for(;;){ /* �󔒂̃X�L�b�v */
	    if( i >= pos )
		goto exit;
	    if( !isspace(buffer[i] & 255) )
		break;
	    ++i;
	}
	top = i;
	for(;;){ /* �P��̃X�L�b�v */
	    if( i >= pos )
		goto exit;
	    if( isspace( buffer[i] & 255) )
		break;
	    ++i;
	}
    }
  exit:
    if( pos > top )
	backward_( pos - top );
    return NEXTCHAR;
}


/* �R�}���h�FCTRL-C�F���̓L�����Z�����邪�A�\���͏����Ȃ� */
Status GetLine::abort(int)
{
    buffer.erase_line(0);
    return CANCEL;
}

/* �R�}���h�FCTRL-D�F�J�[�\����̕������폜���邩�A�⊮�Ώۈꗗ���o�� */
Status GetLine::erase_or_listing(int key)
{
    return pos == buffer.length() ? this->listing(key) : this->erase(key);
}

/* �R�}���h�FCTRL-D�F
 *     ���͕����� 0 ������ EOF �Ƃ݂Ȃ��ďI��
 *     �s���� �� �⊮�Ώۈꗗ
 *     ���̑� �� �P���폜
 */
Status GetLine::erase_listing_or_bye(int key)
{
    if( buffer.length() == 0 )
	return this->bye(key);
    else if( pos == buffer.length() )
	return this->listing(key);
    else
	return this->erase(key);
}


/* �R�}���h�FCTRL-E�F�J�[�\���𖖔��ֈړ�������B*/
Status GetLine::goto_tail(int)
{
    if( buffer.length()+1-offset >= width ){
        // ��ԍŌ��[width-1]�ɂ͋󔒕������K�v.
        putbs( pos-offset );
        offset = buffer.length()+1-width;
        puts_between( offset , buffer.length() );
        putchr(' ');
        putbs(1);
    }else{
        puts_between( pos , buffer.length() );
    }
    pos = buffer.length() ;
    return NEXTCHAR;
}

/* n �����J�[�\����i�߂� */
void GetLine::foreward_(int n)
{
    int oldpos = pos;
    pos += n;
    int nxtpos = pos + buffer.sizeAt(pos);
    if( nxtpos-offset >= width ){
        // �X�N���[��
        putbs( oldpos-offset );
        offset = nxtpos-width;
        puts_between( offset , nxtpos );
        putbs( nxtpos - pos );
    }else{
        puts_between( oldpos , pos );
    }
    repaint_after();
}

/* �R�}���h�FCTRL-F�F�J�[�\���E�ړ� */
Status GetLine::foreward(int)
{
    if( pos < buffer.length() )
	foreward_( buffer.sizeAt(pos) );
    return NEXTCHAR;
}

Status GetLine::foreward_word(int)
{
    int i=0;
    while( pos+i < buffer.length() && !isspace(buffer[pos+i]) )
	++i;
    while( pos+i < buffer.length() && isspace(buffer[pos+i]) )
	++i;
    foreward_( i );
    return NEXTCHAR;
}


/* �R�}���h�FCTRL-H�F�o�b�N�X�y�[�X */
Status GetLine::backspace(int key)
{
    if( pos <= 0 )
        return NEXTCHAR;
    
    backward(key);
    erase(key);

    return NEXTCHAR;
}

void GetLine::savekillring( int from , int to )
{
    NnString killbuffer;
    
    buffer.decode(from,to-from,killbuffer);
    Console::writeClipBoard( killbuffer.chars() , killbuffer.length() );
}

/* �R�}���h�FCTRL-K�F�J�[�\���ȍ~�̕������폜����B*/
Status GetLine::erase_line(int)
{
    savekillring( pos , buffer.length() );
    putbs( putspc( tail() - pos ) );
    buffer.erase_line( pos );
    return NEXTCHAR;
}

/* �R�}���h�FCTRL-L�F��ʂ��N���A���� */
Status GetLine::cls(int)
{
    clear();
    prompt();

    puts_between( offset , pos );
    repaint_after();

    return NEXTCHAR;
}

/* �R�}���h�FCTRL-M�F���� */
Status GetLine::enter(int)
{
    return NEXTLINE;
}

/* �R�}���h�FCTRL_N�F�q�X�g���Q�Ɓi���������j*/
Status GetLine::next(int)
{
    if( history.size() <= 0 )
        return NEXTCHAR;

    if( history_pointor == history.size()-1 )
	buffer.decode( history[ history_pointor ]->body() );

    if( ++history_pointor >= history.size() )
        history_pointor = 0;

    replace_all_repaint( history[ history_pointor ]->body().chars() );

    return NEXTCHAR;
}


/* �R�}���h�FCTRL-P�F�q�X�g���Q�� */
Status GetLine::previous(int)
{
    if( history.size() <= 0 )
        return NEXTCHAR;

    if( history_pointor == history.size()-1 )
	buffer.decode( history[ history_pointor ]->body() );

    if( --history_pointor < 0 )
        history_pointor = history.size()-1;

    replace_all_repaint( history[ history_pointor ]->body().chars() );

    return NEXTCHAR;
}

/* �R�}���h�FVz ���C�N�ȃq�X�g���Q�Ɓi�����j*/
Status GetLine::vz_next(int key)
{
    return this->vz_previous(key);
}

/* �R�}���h�FVz ���C�N�ȃq�X�g���Q�Ɓi�ߋ��j */
Status GetLine::vz_previous(int key)
{
    int at,size;
    NnString curword=current_word(at,size);
    // printf("\n(%d,%d)\n",at,size);
    NnString found;

    int m=history.size()-1;
    if( m < 0 )
        return NEXTCHAR;

    int n=0;
    int sealsize=0;
    NnString temp;
    /*** �s�P�ʂ̌��� ***/
    if( at <= 0 ){
	buffer.decode(temp);
        /* �s�P�ʂ� Vz ���C�N�ȃq�X�g���Q�� */
        if( ( which_command(key) == &GetLine::vz_previous
            ? seekLineForward(m,temp.chars() )
            : seekLineBackward(m,temp.chars() ) ) < 0 )
            return NEXTCHAR;
        for(;;){
            sealsize = printSeal( history[m]->body().chars()+buffer.length() , sealsize );
            key=getkey();
            if( which_command(key) == &GetLine::vz_previous ){
		/* ���ɓ��͂��ꂽ�R�}���h���ߋ������̏ꍇ */
                if( seekLineForward(m,temp.chars() ) < 0 )
                    break; /* �����ł��Ȃ������ꍇ�A�I�� */
            }else if( which_command(key) == &GetLine::vz_next ){
		/* ���ɓ��͂��ꂽ�R�}���h�����������̏ꍇ */
                if( seekLineBackward(m,temp.chars() ) < 0 )
                    break;
            }else{
		/* ���ɓ��͂��ꂽ�R�}���h�������ȊO�������ꍇ */
		eraseSeal( sealsize );
                insertHere( history[m]->body().chars()+buffer.length() );
		/* ���͂��ꂽ�L�[���̂����߂������ďI�� */
                return interpret(key);
            }
        }
	eraseSeal( sealsize );
        return NEXTCHAR;
    }
    
    /*** �P��P�ʂ̌��� ***/
    if( ( which_command(key) == &GetLine::vz_previous
        ? seekWordForward(m,n,curword,found) 
        : seekWordBackward(m,n,curword,found) ) < 0 )
    {
	/* ����}�b�`���Ȃ��ꍇ�͉��������ɏI������ */
        return NEXTCHAR;
    }

    for(;;){
        /* �P��P�ʂ̃q�X�g���Q�� */
        sealsize = printSeal( found.chars() + curword.length() , sealsize );
        key=getkey();
        if( which_command(key) == &GetLine::vz_previous ){
	    /* ���ɓ��͂����L�[�́A������ */
            if( seekWordForward(m,n,curword,found) < 0 )
                break;
        }else if( which_command(key) == &GetLine::vz_next ){
	    /* ���ɓ��͂����L�[�́A�O���� */
            if( seekWordBackward(m,n,curword,found) < 0 )
                break;
        }else{
	    /* ������ł��Ȃ��c�m�聕�L�[���� */
	    eraseSeal( sealsize );
            insertHere( found.chars() + curword.length() );
            return interpret(key);
        }
    }
    eraseSeal( sealsize );
    return NEXTCHAR;
}

/* �R�}���h�FCTRL-T�F���O�̕��������ւ���B*/
Status GetLine::swap_char(int)
{
    char tmp[ 4 ];
    int  i;

    if( pos == 0 )
	return NEXTCHAR;

    if( pos >= buffer.length() )
	backward(0);
    backward(0);

    for(i=0;i<buffer.sizeAt(pos);++i)
	tmp[ i ] = buffer[ pos+i ];
    tmp[ i ] = '\0';
    buffer.delroom( pos , i );
    buffer.insert( tmp , pos + buffer.sizeAt(pos) );
    repaint_after( 0 );

    foreward(0);
    foreward(0);

    return NEXTCHAR;
}
/* �R�}���h�FCTRL-U�F�擪����J�[�\�����O�܂ł��폜 */
Status GetLine::erase_before(int)
{
    putbs( pos - head() );
    savekillring( 0 , pos );
    buffer.delroom( 0 , pos );
    int bs=pos;
    offset = pos = 0;
    repaint_after( bs );
    return NEXTCHAR;
}

/* �R�}���h�FCTRL-V�F�R���g���[���L�[��}������ */
Status GetLine::insert_ctrl(int)
{
    return insert(getkey());
}

/* �R�}���h�FCtrl-W�F�J�[�\�����O�̒P������� */
Status GetLine::erase_word(int)
{
    if( pos <= 0 )
        return NEXTCHAR;
    
    int at=0,bs;

    int spc=1;
    for(int i=0; i<pos; i++){
        int spc_=isSpace(buffer[i]);
        if( spc && !spc_ )
            at = i; 
        spc = spc_;
    }
    bs = pos - at;
    if( at > offset ){
        putbs( bs );
    }else{
        putbs( pos - offset );
        offset = at;
    }
    savekillring( at , pos );
    buffer.delroom( at , pos - at );
    pos -= bs;
    repaint_after( bs );
    return NEXTCHAR;
}

/* �R�}���h�FCTRL-Y�F�y�[�X�g */
Status GetLine::yank(int)
{
    NnString killbuffer;

    Console::readClipBoard( killbuffer );

    if( killbuffer.at( killbuffer.length()-1 ) == '\n' )
        killbuffer.chop();
    if( killbuffer.at( killbuffer.length()-1 ) == '\r' )
        killbuffer.chop();

    insertHere( killbuffer.chars() );
    
    return NEXTCHAR;
}


/* �R�}���h�FCTRL-Z�FNYA*OS�I�� */
Status GetLine::bye(int)
{
    return TERMINATE;
}

/* �R�}���h�FCtrl-[�F�S�������폜���� */
Status GetLine::erase_all(int)
{
    /* �J�[�\���ȍ~�̕��������� */
    putspc( tail() - pos );
    putbs( tail() - head() );
    /* �J�[�\���ȑO�̕����������āA�v�����v�g�̒���փJ�[�\�����ړ� */
    putbs( putspc( pos - head() ) );
    
    offset = pos = 0;
    buffer.erase_line( 0 );
    return NEXTCHAR;
}


/* �R�}���h�F���͖��� */
Status GetLine::do_nothing(int)
{
    return NEXTCHAR;
}

/* �R�}���h�F�\���\������}������ */
Status GetLine::insert(int key)
{
    int oldpos = pos;
    int size1=buffer.insert1(key,pos);
    if( size1 <= 0 ){ // memory allocation error.
        return NEXTCHAR;
    }
    pos += size1;
    int nxtpos = pos + buffer.sizeAt(pos);
    // �\���̍X�V.
    if( nxtpos-offset > width ){
        // �E�[�𒴂������ȂƂ�.
        putbs( oldpos-offset );
        offset = nxtpos-width;
        puts_between( offset , nxtpos );
        putbs( nxtpos - pos );
    }else{
        puts_between( oldpos , pos );
    }
    repaint_after();
    return NEXTCHAR;
}

/* �R�}���h�FDEL�F�J�[�\����̕������폜 */
Status GetLine::erase(int)
{
    repaint_after( buffer.erase1(pos) );
    return NEXTCHAR;
}

Status GetLine::ime_toggle(int)
{
#ifdef NYACUS
  /* �L�[�������ʂ𔽉f�����邽�߂̑҂��i�茳�̊��ł͒P�Ȃ� Yield ������ 
     0 �ł����Ȃ������������B���ɂ���Ă͂����Ƒ傫�߂̒l�����邩���j */
  const DWORD dwYieldMSecForKey = 1;
  
  keybd_event(VK_MENU, 0 /* 0x38 */, 0, 0);                 /* down Alt */
  keybd_event(VK_KANJI, 0 /* 0x29 */, 0, 0);                /* down Kanji */
  keybd_event(VK_MENU, 0 /* 0x38 */, KEYEVENTF_KEYUP, 0);   /* up Alt */
  keybd_event(VK_KANJI, 0 /* 0x29 */, KEYEVENTF_KEYUP, 0);  /* up Kanji */
  Sleep(dwYieldMSecForKey);  /* wait for a proof */
#endif
  return NEXTCHAR;
}
