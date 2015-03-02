#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "nnhash.h"

/* �ėp�Ɏg���A�v���p�e�B�[�N���X */
NnHash properties;

/* Pair - �n�b�V���e�[�u���̗v�f : �L�[�ƒl�̃y�A�Ɋւ���N���X */
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

/* HashTable ������������
 * return
 *	0 ... ����
 *     -1 ... ���s
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
 *	key �̒l�Ƃ��� rep ��o�^����B
 *	key == 0 �̎��A�폜����B
 * return
 *       0 : ����
 *      -1 : �������G���[(�e�[�u���S�̂̊m�ۂ̎��s)
 *      -2 : �������m�ۃG���[(�v�f�P��)
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
            /* �L�[�������I */
            if( obj != NULL ){
                /* �l����NULL�̏ꍇ�A�v�f��u�� */
                cur->replace( obj );
            }else{
                /* �l�� NULL �̏ꍇ�A�v�f�̍폜 */
                *pre = cur->next();
                delete cur;
            }
            return 0;
        }
        if( cur->next() == NULL ){
            /* �V�K�I�u�W�F�N�g */
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


/* �A�z�z�����������B
 *	key �L�[�l 
 * return
 *	��NULL �c �I�u�W�F�N�g�ւ̃|�C���^
 *	NULL   �c �}�b�`����I�u�W�F�N�g�͖��������B
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

/* �f�X�g���N�^
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

/* �n�b�V���p�J�[�\��������
 *	hash - �n�b�V��
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

/* �����z��ɑ΂���C���f�b�N�X����i�߂�.
 * ��̃C���f�b�N�X�͓ǂݔ�΂�.
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

/* �n�b�V���p�J�[�\������i�߂�. */
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

/* ���ϐ��̒l�𓾂�(���ϐ��̑啶��/����������ʂ��Ȃ�)
 *    var - ���ϐ���
 *    none - ���ϐ������݂��Ȃ����ɕԂ��l.
 * return
 *     ���ϐ��̒l
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
