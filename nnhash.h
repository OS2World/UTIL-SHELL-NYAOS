/* NnHash :
 *   ������(NnString,char*)�������L�[�Ƃł���n�b�V���e�[�u���B
 *   ������Ɋ֘A�t���ēo�^����I�u�W�F�N�g�� NnObject ��
 *   �h���N���X�łȂ���΂Ȃ�Ȃ��B
 */
#ifndef NNHASH_H
#define NNHASH_H

#include "nnstring.h"

/** �n�b�V���e�[�u���̃I�u�W�F�N�g */
class NnHash : public NnObject {
public:
    class Each;
    /** �L�[�ƒl�̃y�A��ێ�����I�u�W�F�N�g  */
    class Pair : public NnObject {
	Pair *next_;
	NnString key_;
	NnObject *value_;

	void replace( NnObject * );
	Pair *connect( Pair * );
	Pair *next() { return next_; }

	Pair( const NnString &k , NnObject *obj=0 )
	    : next_(0) , key_(k) , value_(obj){}
    public:
	~Pair();

	/** �L�[��Ԃ� */
	NnString &key()  { return key_;   }
	/** �L�[�Ɋ֘A����ꂽ�I�u�W�F�N�g��Ԃ� */
	NnObject *value(){ return value_; }

	friend class Each;
	friend class NnHash;
    };
    /* �C�^���[�^ */
    class Each : public ::NnEnum {
	const NnHash &hash;
	Pair  *ptr;
	int index;
	void nextindex();
    public:
	/** �R���X�g���N�^�F�񋓂����n�b�V���e�[�u�����w�肷�� */
	Each( const NnHash &hash );

	/** ���ݍ����Ă���A�L�[�ƃI�u�W�F�N�g�̃y�A(Pair)�ւ�
	 *  �|�C���^��Ԃ��B�߂�l�̌^�� NnObject* �Ȃ̂́A
	 *  Super�N���X�� NnEnum �ƌ^�����킹�邽�߁B
	 *  �����ׂ��y�A���Ȃ��ꍇ�� NULL ���Ԃ�B
	 */
	virtual   NnObject *operator *();

	/** ���̗v�f�ֈړ�����B���͂�v�f���Ȃ��ꍇ�́A
	 *  operator * �ɂ� NULL ���Ԃ�B
	 */
	virtual   void      operator++();

	/** ���ݍ����Ă���A�v�f�փA�N�Z�X����
	 *  �L�[�̏ꍇ�F        (*this)->key()    (NnString&�^)
	 *  �I�u�W�F�N�g�̏ꍇ�F(*this)->value()  (NnObject*�^)
	 */
	Pair *operator->(){ return ptr; }
	friend class NnHash;
    };
private:
    Pair **table;
    int size;
    int init();
public:
    int put_(const NnString &key, NnObject *obj);
    /** key �Ɋ֘A����ꂽ�I�u�W�F�N�g�ւ̃|�C���^��Ԃ��B
     *  ���݂��Ȃ���΁ANULL ��Ԃ��B
     */
    NnObject *get(const NnString &key);
    NnObject *get(const char *key);

    /** key �Ɋ֘A���āAobj ��o�^����B
     *  obj �� delete���� NnHash ���Ɉړ�����B
     */
    int  put(const NnString &key, NnObject *obj)
	{ return obj ? put_(key,obj) : -3 ; }

    /** key �Ɋ֘A�t����ꂽ�I�u�W�F�N�g���폜����B
     *  �I�u�W�F�N�g�� delete �����
     */
    void remove(const NnString &key)
	{ put_(key,0); }

    /** �R���X�g���N�^ */
    NnHash(int s=256) : table(0) , size(s) { }
    NnHash( const NnHash & );
    ~NnHash();

    NnObject *clone() const { return new NnHash(*this); }
    friend class Each;

#if 0
    class Each {
	char *p;
    public:
	Each( const char *p_ ) : p(p_){}
	int more() const { return *p != '\0'; }
	void next(){ if( isKanji(*p) ){ ++p; } ++p; }
    };
#endif
};

extern NnHash properties;

#endif
