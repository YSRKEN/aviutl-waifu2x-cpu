/* SIMD�Ɋ֌W���镔����ʋL */

/* �v���v���Z�b�T */
#include <immintrin.h>
#define PackedSet1    _mm256_set1_ps
#define PackedSetZero _mm256_setzero_ps
#define PackedAdd     _mm256_add_ps
#define PackedMul     _mm256_mul_ps
#define PackedLoad    _mm256_loadu_ps
#define PackedStore   _mm256_storeu_ps
#define PackedBrend   _mm256_blendv_ps
#define Alignment     __declspec(align(32))

/* typedef�錾 */
typedef __m256 PackedFloat;

/* �萔�錾 */
const auto alignment_size = 32;

/* �C�����C���֐� */
// �Ԃ�l��A * B + C
inline PackedFloat PackedFMA(const PackedFloat A, const PackedFloat B, const PackedFloat C){
	return PackedAdd(PackedMul(A, B), C);
}
inline PackedFloat PackedCmpLt(const PackedFloat A, const PackedFloat B) {
	return _mm256_cmp_ps(A, B, _CMP_LT_OS);
}
