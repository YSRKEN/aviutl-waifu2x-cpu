/* SIMDに関係する部分を別記 */

/* プリプロセッサ */
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
#define PackedStore   _mm_store_ps
#define Alignment     __declspec(align(16))

/* typedef宣言 */
typedef __m128 PackedFloat;

/* 定数宣言 */
const auto alignment_size = 16;

/* インライン関数 */
// 返り値＝A * B + C
inline PackedFloat PackedFMA(const PackedFloat A, const PackedFloat B, const PackedFloat C){
	return PackedAdd(PackedMul(A, B), C);
}
