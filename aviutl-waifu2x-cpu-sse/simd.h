/* SIMD�Ɋ֌W���镔����ʋL */

/* �v���v���Z�b�T */
#include <xmmintrin.h>
#define PackedSet1    _mm_set1_ps
#define PackedSetZero _mm_setzero_ps
#define PackedAdd     _mm_add_ps
#define PackedMul     _mm_mul_ps
#define PackedCmpLt   _mm_cmplt_ps
#define PackedCmpGe   _mm_cmpge_ps
#define PackedAnd     _mm_and_ps
#define PackedOr      _mm_or_ps
#define PackedLoad    _mm_loadu_ps
#define PackedStore   _mm_storeu_ps
#define Alignment     __declspec(align(16))

/* typedef�錾 */
typedef __m128 PackedFloat;

/* �萔�錾 */
const auto alignment_size = 16;

/* �C�����C���֐� */
// �Ԃ�l��A * B + C
inline PackedFloat PackedFMA(const PackedFloat A, const PackedFloat B, const PackedFloat C){
	return PackedAdd(PackedMul(A, B), C);
}
