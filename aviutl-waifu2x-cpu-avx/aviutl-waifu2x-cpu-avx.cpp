/* waifu2x-cpu Ver.1.4 by YSR */

/* プリプロセッサ */
#pragma warning( disable: 4018)
//C標準ライブラリ
#include <cstdint>
// STL
#include <chrono>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
// Windows依存
#include <tchar.h>
#include <windows.h>
// SIMD関係
#include "simd.h"
// AviUtl関係
#include "filter.h"

#define kSoftName "waifu2x-cpu[AVX]"

/* using宣言 */
using std::stod;
using std::stoi;
using std::string;
using std::vector;

/* Typedef宣言 */
typedef uint32_t Int;

/* 定数宣言 */
// UIにおける設定
//トラックバー(数・名前・初期値・下限値・上限値を設定する)
const int kTracks = 5;
TCHAR *track_name[] = { "thread", "noise", "scale", "block_x", "block_y" };
int   track_default[] = { 1, 0, 0, 32, 32 };
int   track_s[] = { 1, 0, 0, 32, 32 };
int   track_e[] = { 32, 2, 1, 512, 512 };
enum kTrackBar { kTrackThread, kTrackNoise, kTrackScale, kTrackBlockX, kTrackBlockY };
enum kModelKind { kModelDenoise1, kModelDenoise2, kModelScale2x, kModels };
//チェックボックス(数・名前・初期値を設定する)
const int kChecks = 2;
TCHAR *check_name[] = { "use blocking", "photography" };
int	  check_default[] = { 0, 0 };
enum kCheckBox { kCheckBlock, kCheckPhoto };
// ソフトウェアにおける設定
const auto kSteps = 7;		//ステップ数
const auto kMaxInput = 128;	//入力平面の最大数
const auto kMaxOutput = 128;	//出力平面の最大数
const auto kWidthSize = 3;		//畳み込みする重みの横サイズ
const auto kHeightSize = 3;		//畳み込みする重みの縦サイズ
const auto kFilterSize = kWidthSize * kHeightSize;		//畳み込みする重みの全体サイズ
const auto kRGB = 3;
const auto SIMD = sizeof(PackedFloat) / sizeof(float);	//SIMDにおける処理幅
const PackedFloat kZeroSIMD = PackedSetZero();			//比較用の値
const PackedFloat kConstSIMD = PackedSet1(0.1);			//負数だけ0.1を掛けるための値

/* クラス・構造体定義 */
// フィルタDLL用構造体
FILTER_DLL filter = {
	FILTER_FLAG_EX_INFORMATION, 0, 0, kSoftName,
	kTracks, track_name, track_default, track_s, track_e,
	kChecks, check_name, check_default,
	func_proc, func_init, NULL, NULL, NULL,
	NULL, NULL,
	NULL, NULL,
	"waifu2x-cpu version 1.4 by YSR",
	NULL, NULL,
};
// 1ステップにおけるデータ
struct Step{
	Int output_plane_size;
	Int input_plane_size;
	PackedFloat weight_simd[kMaxOutput][kMaxInput][kFilterSize];
	PackedFloat bias[kMaxOutput];
};
// 1モデルにおけるデータ
struct Model {
	Step step[kSteps];
	// 初期化関数
	void Init(const string filename) {
		// ファイルが開けないとアウト
		std::ifstream fin(filename, std::ios_base::in | std::ios_base::binary);
		if (fin.fail()) throw filename;
		// 読み込みループ
		for (auto s = 0; s < kSteps; ++s) {
			Step *now_step = &step[s];	//現在のStepを指すポインタ
			// inputおよびoutput
			fin.read(reinterpret_cast<char*>(&now_step->input_plane_size), sizeof(Int));
			fin.read(reinterpret_cast<char*>(&now_step->output_plane_size), sizeof(Int));
			// weight
			for (auto o = 0; o < now_step->output_plane_size; ++o) {
				for (auto i = 0; i < now_step->input_plane_size; ++i) {
					PackedFloat *now_weight_simd = now_step->weight_simd[o][i];
					for (auto k = 0; k < kFilterSize; ++k) {
						float temp;
						fin.read(reinterpret_cast<char*>(&temp), sizeof(float));
						now_weight_simd[k] = PackedSet1(temp);
					}
				}
			}
			// bias
			for (auto o = 0; o < now_step->output_plane_size; ++o) {
				float temp;
				fin.read(reinterpret_cast<char*>(&temp), sizeof(float));
				now_step->bias[o] = PackedSet1(temp);
			}
		}
	}
};
//独自アロケータ
template<class T>
class AllocSIMD : public std::allocator <T> {
public:
	AllocSIMD() { }
	AllocSIMD(const AllocSIMD& x) { }
	template<class U>
	AllocSIMD(const AllocSIMD<U>& x) { }
	pointer allocate(size_type n, const_pointer hint = 0) {
		T* return_ptr = (T*)_mm_malloc(n * sizeof(T), alignment_size);
		if (return_ptr == NULL) throw std::bad_alloc();
		return (pointer)return_ptr;
		return (pointer)_mm_malloc(n * sizeof(T), alignment_size);
	}
	void deallocate(pointer ptr, size_type n) {
		_mm_free(ptr);
	}
	template<class U>
	struct rebind { typedef AllocSIMD<U> other; };
};

/* プロトタイプ宣言 */
// StretchNN(フィルタPROC用構造体へのポインタ)
void StretchNN(FILTER_PROC_INFO*);
// SetFilter(フィルタPROC用構造体へのポインタ, 処理する際に使うモデルデータの番号, スレッド数, 分割時のブロックサイズ<, 〃Y)
void SetFilter(FILTER_PROC_INFO*, const int, const int, const int, const int);

/* グローバル変数宣言 */
/* こんなことはしたくなかったんや！
* でも、1フレーム毎に1回モデルデータを読み出すなんて
* 馬鹿げたことはしたくなかったんや！
* 誰か私にもっと上手い対処法を教えてくれ……
*/
Model g_model_data[kModels * 2];

/* AviUtlから呼び出すための関数 */
EXTERN_C FILTER_DLL __declspec(dllexport) * __stdcall GetFilterTable(void) {
	return &filter;
}

/* 初期化関数 */
BOOL func_init(FILTER *fp) {
	try {
		g_model_data[kModelDenoise1].Init(".\\plugins\\models\\noise1_model_2d.dat");
		g_model_data[kModelDenoise2].Init(".\\plugins\\models\\noise2_model_2d.dat");
		g_model_data[kModelScale2x].Init(".\\plugins\\models\\scale2.0x_model_2d.dat");
		g_model_data[kModelDenoise1 + kModels].Init(".\\plugins\\models\\noise1_model_3d.dat");
		g_model_data[kModelDenoise2 + kModels].Init(".\\plugins\\models\\noise2_model_3d.dat");
		g_model_data[kModelScale2x + kModels].Init(".\\plugins\\models\\scale2.0x_model_3d.dat");
	}
	catch (string err_msg_) {
		string err_msg = err_msg_ + "を読み込む際にエラーが発生しました。";
		MessageBox(NULL, err_msg.c_str(), kSoftName, MB_OK);
		return FALSE;
	}
	return TRUE;
}

/* 処理関数 */
BOOL func_proc(FILTER *fp, FILTER_PROC_INFO *fpip) {
	try {
		// 計算するする場合としない場合とで、タイトルバーの表示を変更する
		if (!fp->exfunc->is_saving(fpip->editp)) {
			if ((fp->track[kTrackNoise] == 0) && (fp->track[kTrackScale] == 0)) {
				// 計算しない場合
				SetWindowText(fp->hwnd, _T(kSoftName));
				return TRUE;
			}
			else {
				// 計算する場合
				SetWindowText(fp->hwnd, _T("waifu2x-cpu(処理中...)"));
			}
		}

		// 計算する場合、設定に応じてモデルデータを選択し、フィルタ処理を行う
		auto start = std::chrono::system_clock::now();
		//ノイズ除去する場合は、後に拡大する場合でも先に処理する
		if (fp->track[kTrackNoise] > 0) {
			SetFilter(fpip, fp->track[kTrackNoise] - 1 + fp->check[kCheckPhoto] * kModels, fp->track[kTrackThread], fp->check[kCheckBlock] * fp->track[kTrackBlockX], fp->check[kCheckBlock] * fp->track[kTrackBlockY]);
		}
		//拡大する場合は、まず最近傍法で拡大してからフィルタ処理を行う
		if (fp->track[kTrackScale] > 0) {
			StretchNN(fpip);
			SetFilter(fpip, kModelScale2x + fp->check[kCheckPhoto] * kModels, fp->track[kTrackThread], fp->check[kCheckBlock] * fp->track[kTrackBlockX], fp->check[kCheckBlock] * fp->track[kTrackBlockY]);
		}

		// 演算時間をタイトルバーに表示する
		if (!fp->exfunc->is_saving(fpip->editp)) {
			auto end = std::chrono::system_clock::now();
			auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
			std::stringstream title_bar;
			title_bar << "waifu2x-cpu(" << ms << "ms)";
			SetWindowText(fp->hwnd, _T(title_bar.str().c_str()));
		}
	}
	catch (std::bad_alloc) {
		if (!fp->exfunc->is_saving(fpip->editp)) {
			SetWindowText(fp->hwnd, _T(kSoftName));
			MessageBox(NULL, "メモリが確保できませんでした。", kSoftName, MB_OK);
		}
		return FALSE;
	}
	return TRUE;
}

/* 最近傍法による拡大を行う */
void StretchNN(FILTER_PROC_INFO *fpip) {
	// 拡大に使用する処理サイズを決定する
	// (拡大後に画像領域からはみ出ないようにする)
	auto scale_size_x = fpip->w;
	if (scale_size_x * 2 > fpip->max_w) scale_size_x = fpip->max_w / 2;
	auto scale_size_y = fpip->h;
	if (scale_size_y * 2 > fpip->max_h) scale_size_y = fpip->max_h / 2;
	// 最近傍法で、fpip->ycp_editからfpip->ycp_tempに向かって拡大する
	// (単に2倍にしているだけなので楽に記述できる)
	for (auto y = 0; y < scale_size_y; y++) {
		auto ycp_from = fpip->ycp_edit + y     * fpip->max_w;
		auto ycp_to = fpip->ycp_temp + y * 2 * fpip->max_w;
		for (auto x = 0; x < scale_size_x; x++) {
			for (auto k = 0; k < 2; ++k) {
				ycp_to->y = ycp_from->y;
				ycp_to->cb = ycp_from->cb;
				ycp_to->cr = ycp_from->cr;
				ycp_to[fpip->max_w].y = ycp_from->y;
				ycp_to[fpip->max_w].cb = ycp_from->cb;
				ycp_to[fpip->max_w].cr = ycp_from->cr;
				++ycp_to;
			}
			++ycp_from;
		}
	}
	// 現在の画面サイズを変更する
	fpip->w = scale_size_x * 2;
	fpip->h = scale_size_y * 2;
	// 最後にポインタを入れ替える
	auto ycp = fpip->ycp_edit;
	fpip->ycp_edit = fpip->ycp_temp;
	fpip->ycp_temp = ycp;
}

/* フィルタ処理を行う */
/* mode_         …… kModelKindに対応している。0～2がデノイズレベル1・デノイズレベル2・拡大
* thread_       …… 処理するスレッド数
* block_size_x_ …… 処理する際のブロックサイズX。0だと横幅と同じになる
* block_size_y_ …… 処理する際のブロックサイズY。0だと縦幅と同じになる
*/
void SetFilter(FILTER_PROC_INFO *fpip, const int mode_, const int thread_, const int block_size_x_, const int block_size_y_) {
	/* ブロック処理する際のブロックサイズを決定する
	* 0だと横・縦幅と同じになるので、分割しなくても同じ処理を踏ませることになる
	*/
	auto block_size_x = block_size_x_;
	if (block_size_x == 0) block_size_x = fpip->w;
	auto block_size_y = block_size_y_;
	if (block_size_y == 0) block_size_y = fpip->h;

	/* 縦横の、下・右部分のパディングサイズを決定する
	* ・ステップ数をXとした際、パディングは画像の上下左右にX以上無ければならない
	* ・ブロッッキングする場合、画像をブロックに分割した後、パディング込みのデータを処理する必要がある
	* ・SIMD処理の観点から、パディング後の横幅は処理幅(SSE系だと128/32＝4、AVX・軽だと8)で割り切れると美味しい
	*/
	// 右部分のパディングサイズ(調整が必要)
	// 例えば処理幅4で3ドット余れば1ドット付け足し、処理幅8で5ドット余れば3ドット付け足す
	auto padding_x = kSteps;
	auto rightest_block_size = fpip->w % block_size_x;
	if (rightest_block_size == 0) rightest_block_size = block_size_x;
	auto surplus = (kSteps + rightest_block_size + kSteps) % SIMD;
	if (surplus != 0) padding_x += (SIMD - surplus);
	//下部分のパディングサイズ(そのままでOK)
	auto padding_y = kSteps;

	/* 決定したパディングサイズに従いパディングする */
	auto padded_picture_x = kSteps + fpip->w + padding_x;
	auto padded_picture_y = kSteps + fpip->h + padding_y;
	/*PIXEL *pixelp;
	filter.exfunc->rgb2yc(fpip->ycp_edit, pixelp, fpip->w);*/
	// RGB成分を[0,1]に正規化する
	vector<float> padded_picture(kRGB * padded_picture_y * padded_picture_x);
	for (auto y = 0; y < fpip->h; y++) {
		auto ycp = fpip->ycp_edit + y * fpip->max_w;
		for (auto x = 0; x < fpip->w; x++) {
			// 変換式ソース：
			// 「AviUtlの内部形式について」
			// http://web.archive.org/web/20130313184604/http://vm104.xen.klab.org/doc/aviutlyc.html
			// 「Aviutlの内部形式とx264guiExの色空間変換について」
			// http://rigaya34589.blog135.fc2.com/ より「Aviutl/x264guiExの色変換」から入る
			short r = (255 * ycp->y + (((22881 * ycp->cr >> 16) + 3) << 10)) >> 12;
			short g = (255 * ycp->y + (((-5616 * ycp->cb >> 16) + (-11655 * ycp->cr >> 16) + 3) << 10)) >> 12;
			short b = (255 * ycp->y + (((28919 * ycp->cb >> 16) + 3) << 10)) >> 12;
			float normalized_r = 1.0f * r / 255;
			if (normalized_r < 0.0f) normalized_r = 0.0f;
			if (normalized_r > 1.0f) normalized_r = 1.0f;
			padded_picture[(y + kSteps) * padded_picture_x + x + kSteps] = normalized_r;
			float normalized_g = 1.0f * g / 255;
			if (normalized_g < 0.0f) normalized_g = 0.0f;
			if (normalized_g > 1.0f) normalized_g = 1.0f;
			padded_picture[(padded_picture_y + y + kSteps) * padded_picture_x + x + kSteps] = normalized_g;
			float normalized_b = 1.0f * b / 255;
			if (normalized_b < 0.0f) normalized_b = 0.0f;
			if (normalized_b > 1.0f) normalized_b = 1.0f;
			padded_picture[(padded_picture_y * 2 + y + kSteps) * padded_picture_x + x + kSteps] = normalized_b;
			ycp++;
		}
	}
	// 辺の部分を拡張する(途中、テンポラリな変数を挟むことで高速化を図った)
	for (int rgb = 0; rgb < kRGB; ++rgb){
		int offset = rgb * padded_picture_y * padded_picture_x;
		//左上
		auto temp = padded_picture[offset + kSteps * padded_picture_x + kSteps];
		for (auto y = 0; y < kSteps; ++y) {
			for (auto x = 0; x < kSteps; ++x) {
				padded_picture[offset + y * padded_picture_x + x] = temp;
			}
		}
		//右上
		temp = padded_picture[offset + kSteps * padded_picture_x + fpip->w + kSteps - 1];
		for (auto y = 0; y < kSteps; ++y) {
			for (auto x = fpip->w + kSteps; x < padded_picture_x; ++x) {
				padded_picture[offset + y * padded_picture_x + x] = temp;
			}
		}
		//右下
		temp = padded_picture[offset + (fpip->h + kSteps - 1) * padded_picture_x + fpip->w + kSteps - 1];
		for (auto y = fpip->h + kSteps; y < padded_picture_y; ++y) {
			for (auto x = fpip->w + kSteps; x < padded_picture_x; ++x) {
				padded_picture[offset + y * padded_picture_x + x] = temp;
			}
		}
		//左下
		temp = padded_picture[offset + (fpip->h + kSteps - 1) * padded_picture_x + kSteps];
		for (auto y = fpip->h + kSteps; y < padded_picture_y; ++y) {
			for (auto x = 0; x < kSteps; ++x) {
				padded_picture[offset + y * padded_picture_x + x] = temp;
			}
		}
		//上
		auto *temp_p = &padded_picture[offset + kSteps * padded_picture_x];
		for (auto y = 0; y < kSteps; ++y) {
			for (auto x = kSteps; x < fpip->w + kSteps; ++x) {
				padded_picture[offset + y * padded_picture_x + x] = temp_p[x];
			}
		}
		//右
		for (auto y = kSteps; y < fpip->h + kSteps; ++y) {
			temp = padded_picture[offset + y * padded_picture_x + fpip->w + kSteps - 1];
			for (auto x = fpip->w + kSteps; x < padded_picture_x; ++x) {
				padded_picture[offset + y * padded_picture_x + x] = temp;
			}
		}
		//下
		temp_p = &padded_picture[offset + (fpip->h + kSteps - 1) * padded_picture_x];
		for (auto y = fpip->h + kSteps; y < padded_picture_y; ++y) {
			for (auto x = kSteps; x < fpip->w + kSteps; ++x) {
				padded_picture[offset + y * padded_picture_x + x] = temp_p[x];
			}
		}
		//左
		for (auto y = kSteps; y < fpip->h + kSteps; ++y) {
			temp = padded_picture[offset + y * padded_picture_x + kSteps];
			for (auto x = 0; x < kSteps; ++x) {
				padded_picture[offset + y * padded_picture_x + x] = temp;
			}
		}
	}

	/* パディング結果をブロックに分割して処理する */
	// 分割数を計算
	auto block_num_x = fpip->w / block_size_x;
	if (fpip->w % block_size_x != 0) ++block_num_x;
	auto block_num_y = fpip->h / block_size_y;
	if (fpip->h % block_size_y != 0) ++block_num_y;
	// 縦方向における入力サイズ(input_size_y)と出力サイズ(output_size_y)を決定する
	auto input_size_y = block_size_y + kSteps * 2;
	auto output_size_y = block_size_y;
	// ループを回す
	for (auto block_pos_y = 0; block_pos_y < block_num_y; ++block_pos_y) {
		// 一番下のブロック行だけ、入力サイズを調節する
		if (block_pos_y == block_num_y - 1) {
			output_size_y = fpip->h - block_pos_y * block_size_y;
			input_size_y = output_size_y + kSteps * 2;
		}
		// 横方向における入力サイズ(input_size_x)と出力サイズ(output_size_x)を決定する
		auto input_size_x = block_size_x + kSteps * 2;
		if (input_size_x % SIMD != 0) input_size_x += SIMD - (input_size_x % SIMD);
		auto input_size_x_SIMD = input_size_x / SIMD;
		auto output_size_x = block_size_x;
		for (auto block_pos_x = 0; block_pos_x < block_num_x; ++block_pos_x){
			// 一番右のブロック列だけ、入力サイズを調節する
			if (block_pos_y == block_num_y - 1) {
				output_size_x = fpip->w - block_pos_x * block_size_x;
				input_size_x = output_size_x + kSteps * 2;
				if (input_size_x % SIMD != 0) input_size_x += SIMD - (input_size_x % SIMD);
				input_size_x_SIMD = input_size_x / SIMD;
			}
			/* 入力部分 */
			vector<vector<float>> input_picture_rgb(input_size_y, vector<float>(kMaxInput * input_size_x));
			for (int rgb = 0; rgb < kRGB; ++rgb){
				for (auto y = 0; y < input_size_y; y++) {
					for (auto x = 0; x < input_size_x; ++x) {
						input_picture_rgb[y][rgb * input_size_x + x] = padded_picture[(rgb * padded_picture_y + block_pos_y * block_size_y + y) * padded_picture_x + (block_pos_x * block_size_x + x)];
					}
				}
			}
			/* 演算部分 */
			// 縦サイズはステップ毎に2づつ減っていくが横サイズは減らない。
			// これは、横サイズを折角SIMD向けに処理幅で割り切れるようにしたのに潰されたくないため。
			vector<vector<PackedFloat, AllocSIMD<PackedFloat>>> output_picture_rgb(input_size_y, vector<PackedFloat, AllocSIMD<PackedFloat>>(input_size_x_SIMD * kMaxOutput));
			auto input_size_y_ = input_size_y - 2;
			for (auto s = 0; s < kSteps; ++s) {
				Step *step = &g_model_data[mode_].step[s];
				auto input_plane_size = step->input_plane_size;
				auto output_plane_size = step->output_plane_size;
				// 出力平面を生成する
#pragma omp parallel for num_threads(thread_)
				for (auto y = 0; y < input_size_y_; ++y) {
					for (auto x = 0, x_SIMD = 0, x_ = 0; x < input_size_x; x += SIMD, ++x_SIMD, x_ += kMaxOutput) {
						for (auto o = 0; o < output_plane_size; ++o) {
							// 出力平面を初期化する
							PackedFloat temp_simd = PackedSetZero();
							// 畳み込み演算する
							for (auto i = 0, i_ = 0; i < input_plane_size; ++i, i_ += input_size_x) {
								// 読み込み
								PackedFloat *weight = step->weight_simd[o][i];
								// 演算・書き込み
								temp_simd = PackedFMA(weight[0], PackedLoad(&input_picture_rgb[y + 0][i_ + x + 0]), temp_simd);
								temp_simd = PackedFMA(weight[1], PackedLoad(&input_picture_rgb[y + 0][i_ + x + 1]), temp_simd);
								temp_simd = PackedFMA(weight[2], PackedLoad(&input_picture_rgb[y + 0][i_ + x + 2]), temp_simd);
								temp_simd = PackedFMA(weight[3], PackedLoad(&input_picture_rgb[y + 1][i_ + x + 0]), temp_simd);
								temp_simd = PackedFMA(weight[4], PackedLoad(&input_picture_rgb[y + 1][i_ + x + 1]), temp_simd);
								temp_simd = PackedFMA(weight[5], PackedLoad(&input_picture_rgb[y + 1][i_ + x + 2]), temp_simd);
								temp_simd = PackedFMA(weight[6], PackedLoad(&input_picture_rgb[y + 2][i_ + x + 0]), temp_simd);
								temp_simd = PackedFMA(weight[7], PackedLoad(&input_picture_rgb[y + 2][i_ + x + 1]), temp_simd);
								temp_simd = PackedFMA(weight[8], PackedLoad(&input_picture_rgb[y + 2][i_ + x + 2]), temp_simd);
							}
							// バイアスを掛ける
							temp_simd = PackedAdd(temp_simd, step->bias[o]);
							// 負数のみ0.1倍する
							PackedFloat lt_zero = PackedCmpLt(temp_simd, kZeroSIMD);	//各要素について0.0未満なら0xFFFFFFFF、でないと0にする
							output_picture_rgb[y][x_ + o] = PackedBrend(temp_simd, PackedMul(temp_simd, kConstSIMD), lt_zero);
						}
					}
				}
				// 出力平面を入力平面に反映する
#pragma omp parallel for num_threads(thread_)
				for (auto y = 0; y < input_size_y_; ++y) {
					for (auto o = 0, o_ = 0; o < output_plane_size; ++o, o_ += input_size_x) {
						for (auto x = 0, x_SIMD = 0, x_ = 0; x < input_size_x; x += SIMD, ++x_SIMD, x_ += kMaxOutput) {
							PackedStore(&input_picture_rgb[y][o_ + x], output_picture_rgb[y][x_ + o]);
						}
					}
				}
				input_size_y_ -= 2;
			}
			/* 出力部分 */
			for (auto y = 0; y < output_size_y; ++y) {
				int y_ = block_pos_y * block_size_y + y;
				auto ycp = fpip->ycp_edit + y_ * fpip->max_w + block_pos_x * block_size_x;
				for (auto x = 0; x < output_size_x; ++x) {
					int r = static_cast<short>(round(input_picture_rgb[y][x] * 255));
					int g = static_cast<short>(round(input_picture_rgb[y][input_size_x + x] * 255));
					int b = static_cast<short>(round(input_picture_rgb[y][2 * input_size_x + x] * 255));
					ycp->y = ((4918 * r + 354) >> 10) + ((9655 * g + 585) >> 10) + ((1875 * b + 523) >> 10);
					ycp->cb = ((-2775 * r + 240) >> 10) + ((-5449 * g + 515) >> 10) + ((8224 * b + 256) >> 10);
					ycp->cr = ((8224 * r + 256) >> 10) + ((-6887 * g + 110) >> 10) + ((-1337 * b + 646) >> 10);
					ycp++;
				}
			}
		}
	}
	return;
}
