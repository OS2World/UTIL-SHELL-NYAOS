#include <stdlib.h>
#include "nnstring.h"
#include "NnVector.h"

/* �z����g������B
 *	n : �V�T�C�Y
 * return
 *	0 ... ���� , -1 ... ���s
 */
int NnVector::grow(int n)
{
    // if( n <= max_ ) return 0;
    NnObject **newVector = (NnObject**)(array != NULL 
			? realloc( array , n*sizeof(NnObject*) )
			: malloc( n*sizeof(NnObject*) ) );
    if( newVector == NULL )
        return -1;
    
    array = newVector;
    while( max_ < n )
        array[ max_++ ] = NULL;
    return 0;
}

/* �f�X�g���N�^ */
NnVector::~NnVector()
{
    if( array ){
	for(int i=0 ; i<size() ;i++ )
            delete array[i];
	free( array );
    }
}

/* �v�f�𖖔��ɒǉ�����.
 *      obj - �ǉ�����I�u�W�F�N�g
 * return
 *	0 ... ���� 
 *     -1 ... �������m�ێ��s
 *     -2 ... �p�����[�^�G���[(obj �� NULL)
 */
int NnVector::append( NnObject *obj )
{
    if( obj == NULL )
        return -2;
    
    while( size_+1 >= max_ ){
	if( grow(max_+10) != 0 )
	    return -1;
    }
    array[ size_++ ] = obj;
    return 0;
}

/* qsort x NnSortable* ������r�֐�.
 *      x �v�f1(NnObject*�ւ̃|�C���^)
 *      y �v�f2(NnObject*�ւ̃|�C���^)
 * return
 *      �召�֌W��\�������l(x-y�ɑ���)
 */
static int 
compare(const void *x,const void *y)
{
    NnSortable *xs=( *(NnObject**)x )->sortable();
    NnSortable *ys=( *(NnObject**)y )->sortable();
    if( xs == NULL ) 
	return ys != NULL ? -1 : 0;
    if( ys == NULL )
	return +1;
    
    return xs->compare(*ys);
}

/* �\�[�g���s��.
 * �ENnSortable �̃C���X�^���X�łȂ����̂́A
 * �E�z��̑S�v�f�� NnSortable* �ł��邱�Ƃ��g�p�����B
 */
void NnVector::sort()
{
    if( array != NULL  &&  this->size() >= 2 )
        qsort( array , this->size() , sizeof(NnObject*) , &compare );
}

/* �\�[�g���s���B��r�͈����̊֐��I�u�W�F�N�g�ōs��.
 * (�A���S���Y���� selection sort o(n^2) :�ȒP�����A�]�葬���Ȃ�)
 */
void NnVector::sort( NnComparer &comparer )
{
    if( array == NULL ||  this->size() < 2 )
	return;

    for(int i=0 ; i<this->size() ; i++ ){
	for(int j=i+1 ; j<this->size() ; j++ ){
	    if( array[i] == NULL || comparer( array[i] , array[j] ) > 0 ){
		NnObject *tmp=array[i];
		array[i] = array[j];
		array[j] = tmp;
	    }
	}
    }
}

/* at�Ԗڂ̗v�f�������A����߂�B
 * ���s��Asize() �� 1 ����B
 *      at - ��菜���ʒu
 * return
 *      �������I�u�W�F�N�g(delete�͂��Ă��Ȃ�)
 */
NnObject *NnVector::removeAt( int at )
{
    if( at >= size() )
        return NULL;
    NnObject *rc=array[at];
    while( at+1 < size() ){
        array[at] = array[at+1];
        ++at;
    }
    array[--size_] = NULL;
    return rc;
}

/* �d���I�u�W�F�N�g���폜����B
 * ���L�̏����𖞂����K�v������B
 * �E�z��̑S�v�f�� NULL �� NnSortable* �ł���B
 * �E���O�� sort() �����{���Ă���B
 */
void NnVector::uniq()
{
    if( array==NULL || this->size() < 2 )
        return;
    
    while( array[0] == NULL && size() > 1 )
        deleteAt(0);
    
    NnSortable *lhs=array[0]->sortable();
    if( lhs == NULL )
        return;
        
    for(int p=0 ; (p+1)<size() ; p++ ){
        NnSortable *rhs=NULL;
        while(  p+1 < size() 
            && (   array[p+1] == NULL 
                || (rhs=array[p+1]->sortable())== NULL
                || lhs->compare(*rhs)==0 )
        ){
            deleteAt(p+1);
        }
        lhs = rhs;
    }
}
void NnVector::erase()
{
    for(int i=0;i<size_;++i){
	delete array[i];
	array[i] = 0;
    }
    size_ = 0;
}

NnObject *NnVector::clone() const
{
    NnVector *result=new NnVector();
    for(int i=0;i<size();i++){
	result->append( const_at(i)->clone() );
    }
    return result;
}

/* NnString �̃��\�b�h�����ANnVector �����p����鎞�����A
 * �g���Ȃ��̂ŁA������̃\�[�X�ɓ���Ă��� */

/* ��������󔒂ŕ������āA�z��Ɋi�[����.
 *    param �i�[��
 * return
 *    ������
 */
int NnString::splitTo( NnVector &param ) const
{
    int n=0;
    NnString left( *this );
    while( ! left.empty() ){
	NnString *arg1=new NnString();
	left.splitTo(*arg1,left);
	param.append( arg1 );
	++n;
    }
    return n;
}
