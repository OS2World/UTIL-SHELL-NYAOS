#ifndef NNVECTOR_H
#define NNVECTOR_H

#include "nnobject.h"

/* �\�[�g���s�����߂̔�r���s���֐��I�u�W�F�N�g
 */
class NnComparer : public NnObject {
public:
    virtual int operator()( const NnObject *left ,
			    const NnObject *right )=0;
};

/** �ϒ��T�C�Y�̔z��B�v�f�Ƃ��Ċ܂߂���̂́A
 *  NnObject �̔h���N���X�̂݁B
 */
class NnVector : public NnObject {
    NnObject **array;
    int max_,size_;
    int grow(int n);
public:
    NnVector()      : array(0) , max_(0) , size_(0){}
    NnVector(int n) : array(0) , max_(0) , size_(0){ grow(n); }
    ~NnVector();

    /** �v�f�̌���Ԃ� */
    int size() const { return size_; }

    /** i �Ԗڂ̗v�f��Ԃ��B�v�f�� delete �֎~ */
    NnObject *at(int i){ return array[i]; }

    /** i �Ԗڂ̗v�f��Ԃ��B�v�f�͕ύX�֎~ */
    const NnObject *const_at(int i) const { return array[i]; }

    /** �I�u�W�F�N�g�𖖔��֒ǉ�����B
     *  �I�u�W�F�N�g�� delete ���� NnVector �ֈړ�����B*/
    int append( NnObject *obj );

    /** �Ō�̗v�f���폜����B
     * �I�u�W�F�N�g�� delete �����A�߂�l�Ƃ��ĕԂ��B
     *  (delete�������[�U�ֈړ�����)
     */
    NnObject *pop(){ 
	if( size_ <= 0 ) return 0;
	NnObject *rc=array[--size_];
	array[size_]=0;
	return rc;
    }

    /** �ŏ��̗v�f���A�߂�l�Ƃ��ĕԂ��B*/
    NnObject *top(){ return size_ >0 ? array[size_-1] : 0 ; }

    /** x �Ԗڂ̗v�f���폜���A����l�߂�B
     *  �I�u�W�F�N�g�� delete �����A�߂�l�Ƃ��ĕԂ��B
     *  (delete�������[�U�ֈړ�����)
     */
    NnObject *removeAt(int i);

    /** �ŏ��̗v�f���폜���A����l�߂�B
     *  �I�u�W�F�N�g�� delete �����A�߂�l�Ƃ��ĕԂ��B
     *  (delete�������[�U�ֈړ�����)
     */
    NnObject *shift(){ return removeAt(0); }

    /** at �Ԗڂ̗v�f�� delete ���A����l�߂� */
    void deleteAt(int i){ delete removeAt(i); }

    /** �܂܂��v�f���\�[�g����B�召�֌W�̂Ȃ��v�f�́A�����ֈړ������ */
    void sort();
    void sort( NnComparer &comparer );

    /** �A�����铯��v�f�̈���� delete ���A�l�߂� */
    void uniq();

    void erase();
    virtual NnObject *clone() const;
};

#endif
