#ifndef NNOBJECT_H
#define NNOBJECT_H

/** Nihongo Nano class library
 *
 *      NnObject (�I�u�W�F�N�g)
 *        �� NnSortable (��r�\�I�u�W�F�N�g)
 *        ��   �� NnString (������)
 *        �� NnVector  (�ϒ��z��)
 *        �� NnHash (������L�[�̃n�b�V��)
 *        �� NnBullet  (������L�[�ƒl�̃y�A)
 *        �� Writer    (�ďo��)
 *        ��   �� StreamWriter  (FILE*�o��)
 *        ��      �� FileWriter (�t�@�C���o��)
 *        ��      �� PipeWriter (�p�C�v�o��)
 *        �� Reader    (�ē���)
 *        ��   �� StreamReader  (FILE*����)
 *        ��      �� FileReader (�t�@�C������)
 *        ��      �� PipeReader (�p�C�v����)
 *        �� NnEnum    (�C�^���[�^�[)
 *             �� NnHash::Each (�n�b�V���̊e�v�f �� NnBullet )
 *             �� NnDir           (�f�B���N�g��     �� NnString )
 *
 * �Edelete ���錠��/�`�����u���L���v�ƌĂԁB
 *
 * �E�R���N�V�����n�I�u�W�F�N�g�͔�i�[�I�u�W�F�N�g�ɑ΂���
 *   ���L����L����B���������āAauto/static�n�̃I�u�W�F�N�g��
 *   �R���N�V�����n�I�u�W�F�N�g�֓o�^�ł��Ȃ��B
 *
 * �ENnObject �́A�f�X�g���N�^�����z�����Ă���A
 *   �R���N�V�����n�I�u�W�F�N�g�Ɋi�[�����I�u�W�F�N�g��
 *   NnObject ����h�����Ȃ��Ă͂����Ȃ��B
 *
 * �E�����ɓn�����I�u�W�F�N�g�Ɋւ��āA���L�����ړ�����ꍇ��
 *   �|�C���^�n������B
 *      (��)
 *          NnVector vec;
 *          vec.append( new NnString("hogehoge") );
 */

/* �����̐擪�o�C�g�ł���΁A��0 ��Ԃ� */
extern char dbcs_table[ 256 + 128 ];
void init_dbcs_table();

#undef  isKanji
#define isKanji(n) dbcs_table[(unsigned)(128+(n))]

#undef  isSpace
#define isSpace(x) isspace((unsigned)((x)&255))
#undef  isDigit
#define isDigit(x) isdigit((unsigned)((x)&255))
#undef  isAlpha
#define isAlpha(x) isalpha((unsigned)((x)&255))
    
/* �ÓI�z��̗v�f��Ԃ� */
#undef  numof
#define numof(X) (sizeof(X)/sizeof((X)[0]))

/* �ŏ��̕��̒l���O��\���萔�Ƃ��� */
enum{ EXCEPTIONS = (int)(~0u-(~0u >> 1)) };

class NnSortable;

/** NnObject �N���X
 *   �f�X�g���N�^�����z���������N���X�B
 *   ������p������΁ANnVector , NnHash �ɓo�^���邱�Ƃ��ł���B
 */
class NnObject {
public:
    virtual ~NnObject();
    virtual NnObject   *clone() const;
    virtual NnSortable *sortable();
    virtual const char *repr() const { return "<NnObject>"; }
};

/** NnSortable �N���X
 *    ��r�\�ȃI�u�W�F�N�g�͂��ꂩ��p������ƁA
 *    NnVector �ŁAsort,uniq ���\�b�h���g�����Ƃ��ł���B
 */
class NnSortable : public NnObject {
public:
    /** x �Ɣ�r���āA�召�֌W�� ���E��E���̐��l�ŕԂ��B*/
    virtual int compare( const NnSortable &x) const = 0;
    virtual NnSortable *sortable();
};

/** NnEnum �N���X
 *    �v�f������񋓂���C���^�[�t�F�C�X
 */
class NnEnum : public NnObject {
public:
    /** ���̗v�f�ֈړ����� */
    virtual void      operator ++ ()=0;
    void next(){ ++*this; }

    /** ���ݍ����Ă���v�f�ւ̃|�C���^��Ԃ�.
     *  �S�Ă̗v�f�𑖍����I���ƁANULL ��Ԃ�.
     */
    virtual NnObject *operator *  ()=0;

    /** �܂��S�Ă̗v�f�𑖍����Ă��Ȃ���� �^��Ԃ� */
    virtual int more();
};

/* ���̃w�b�_�Ő錾����Ă��郁�\�b�h�̎��̂�
 * NnString.cpp �Œ�`����Ă���B
 */
const char *getEnv( const char *var , const char *none=NULL );

class NnVector;
class NnExecutable : public NnObject{
public:
    virtual int operator()( const NnVector &args )=0;
};

#endif

