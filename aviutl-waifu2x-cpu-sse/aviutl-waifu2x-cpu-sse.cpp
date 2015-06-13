/* waifu2x-cpu Ver.1.2 by YSR */

/* プリプロセッサ */
// STL
#include <chrono>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
// Windows依存
#include <tchar.h>
#include <windows.h>
// SIMD
#include <xmmintrin.h>
// AviUtl関係
#include "filter.h"
#define kSoftName "waifu2x-cpu[SSE]"

const int kTracks = 4;											//トラックバーの数
TCHAR	*track_name[] = {"thread", "noise", "scale", "block"};	//トラックバーの名前
int		track_default[] = {1, 0, 0, 32};						//トラックバーの初期値
int		track_s[] = {1, 0, 0, 32};								//トラックバーの下限値
int		track_e[] = {32, 2, 1, 256};							//トラックバーの上限値

const int kChecks = 1;						//チェックボックスの数
TCHAR	*check_name[] = {"use blocking"};	//チェックボックスの名前
int		check_default[] = {0};				//チェックボックスの初期値 (値は0か1)

/* テンプレ */
FILTER_DLL filter = {
	FILTER_FLAG_EX_INFORMATION,
	0, 0,						//設定ウインドウのサイズ (FILTER_FLAG_WINDOW_SIZEが立っている時に有効)
	kSoftName,					//フィルタの名前
	kTracks,					//トラックバーの数 (0なら名前初期値等もNULLでよい)
	track_name,					//トラックバーの名前郡へのポインタ
	track_default,				//トラックバーの初期値郡へのポインタ
	track_s, track_e,			//トラックバーの数値の下限上限 (NULLなら全て0～256)
	kChecks,					//チェックボックスの数 (0なら名前初期値等もNULLでよい)
	check_name,					//チェックボックスの名前郡へのポインタ
	check_default,				//チェックボックスの初期値郡へのポインタ
	func_proc,					//フィルタ処理関数へのポインタ (NULLなら呼ばれません)
	func_init,					//開始時に呼ばれる関数へのポインタ (NULLなら呼ばれません)
	NULL,						//終了時に呼ばれる関数へのポインタ (NULLなら呼ばれません)
	NULL,						//設定が変更されたときに呼ばれる関数へのポインタ (NULLなら呼ばれません)
	NULL,						//設定ウィンドウにウィンドウメッセージが来た時に呼ばれる関数へのポインタ (NULLなら呼ばれません)
	NULL, NULL,					//システムで使いますので使用しないでください
	NULL,						//拡張データ領域へのポインタ (FILTER_FLAG_EX_DATAが立っている時に有効)
	NULL,						//拡張データサイズ (FILTER_FLAG_EX_DATAが立っている時に有効)
	"waifu2x-cpu version 1.2 by YSR",
	//フィルタ情報へのポインタ (FILTER_FLAG_EX_INFORMATIONが立っている時に有効)
	NULL,						//セーブが開始される直前に呼ばれる関数へのポインタ (NULLなら呼ばれません)
	NULL,						//セーブが終了した直前に呼ばれる関数へのポインタ (NULLなら呼ばれません)
};

EXTERN_C FILTER_DLL __declspec(dllexport) * __stdcall GetFilterTable(void){ return &filter; }

/* using宣言 */
using std::stod;
using std::stoi;
using std::string;
using std::vector;

/* 定数宣言 */
enum kTrackBar { kTrackThread, kTrackNoise, kTrackScale, kTrackBlock};
enum kModelKind { kModelDenoise1, kModelDenoise2, kModelScale2x, kModels };
const int kMaxInput = 128;
const int kMaxOutput = 128;
const int kWidthSize = 3;
const int kHeightSize = 3;
const int kFilterSize = kWidthSize * kHeightSize;
const int x_simd_step = sizeof(__m128) / sizeof(float);

/* クラス定義 */
// 1ステップにおけるデータ
struct Step{
	int output_plane_size;
	int input_plane_size;
	__m128 weight1[kMaxOutput][kMaxInput][kFilterSize];
	__m128 weight2[kMaxOutput][kMaxInput][3];
	vector<float> bias;
};
// 1モデルにおけるデータ
struct Model{
	vector<Step> steps;
	// 初期化関数
	void Init(const string filename){
		// ファイルが開けないとアウト
		std::ifstream fin(filename, std::ios_base::in | std::ios_base::binary);
		if(fin.fail()) throw filename;
		int steps_size;
		//fin >> steps_size;
		fin.read(reinterpret_cast<char*>(&steps_size), sizeof(int));
		steps.resize(steps_size);
		// inputおよびoutput
		for(auto s = 0; s < steps_size; ++s){
			fin.read(reinterpret_cast<char*>(&steps[s].input_plane_size), sizeof(int));
			fin.read(reinterpret_cast<char*>(&steps[s].output_plane_size), sizeof(int));
		}
		// weight
		for(auto s = 0; s < steps_size; ++s){
			for(auto o = 0; o < steps[s].output_plane_size; ++o){
				for(auto i = 0; i < steps[s].input_plane_size; ++i){
					float weight[kFilterSize];
					for(auto k = 0; k < kFilterSize; ++k){
						fin.read(reinterpret_cast<char*>(&weight[k]), sizeof(float));
						steps[s].weight1[o][i][k] = _mm_set1_ps(weight[k]);
					}
					/* 添字の順番：
					* WとHをWHと表す場合、元々は00 01 02 10 11 12 20 21 22と格納していた
					* それをX優先で00 10 20 01 11 21 02 12 22とメモリ上に並べたいので、
					* 0,1,2,3,4,5,6,7,8番目のデータを添字0,3,6,1,4,7,2,5,8番目に置くことに
					*/
					steps[s].weight2[o][i][0] = _mm_set_ps(weight[0], weight[3], weight[6], weight[1]);
					steps[s].weight2[o][i][1] = _mm_set_ps(weight[4], weight[7], weight[2], weight[5]);
					steps[s].weight2[o][i][2] = _mm_set_ps(weight[8], 0.0f, 0.0f, 0.0f);
				}
			}
		}
		// bias
		for(auto s = 0; s < steps_size; ++s){
			steps[s].bias.resize(steps[s].output_plane_size);
			for(auto o = 0; o < steps[s].output_plane_size; ++o){
				fin.read(reinterpret_cast<char*>(&steps[s].bias[o]), sizeof(float));
			}
		}
	}
};

/* プロトタイプ宣言 */
void SetFilter(FILTER_PROC_INFO*, const int, const int);
void SetFilterWithBlocking(FILTER_PROC_INFO*, const int, const int, const int);

/* グローバル変数宣言 */
/* こんなことはしたくなかったんや！
* でも、1フレーム毎に1回モデルデータを読み出すなんて
* 馬鹿げたことはしたくなかったんや！
* 誰か私にもっと上手い対処法を教えてくれ……
*/
vector<Model> g_models(kModels);
auto start = std::chrono::system_clock::now();

/* 初期化関数 */
BOOL func_init(FILTER *fp){
	try{
		g_models[kModelDenoise1].Init(".\\plugins\\models\\noise1_model3.dat");
		g_models[kModelDenoise2].Init(".\\plugins\\models\\noise2_model3.dat");
		g_models[kModelScale2x].Init(".\\plugins\\models\\scale2.0x_model3.dat");
	}
	catch(string err_msg_){
		string err_msg = err_msg_ + "を読み込む際にエラーが発生しました。";
		MessageBox(NULL, err_msg_.c_str(), "waifu2x-cpu", MB_OK);
		return FALSE;
	}
	return TRUE;
}

/* 処理関数 */
BOOL func_proc(FILTER *fp, FILTER_PROC_INFO *fpip){
	/*
	* fp->track[n]			トラックバーの数値
	* fp->check[n]			チェックボックスの数値
	* fpip->w 				実際の画像の横幅
	* fpip->h 				実際の画像の縦幅
	* fpip->max_w			画像領域の横幅
	* fpip->max_h			画像領域の縦幅
	* fpip->ycp_edit		画像領域へのポインタ
	* fpip->ycp_temp		テンポラリ領域へのポインタ
	* fpip->ycp_edit[n].y	画素(輝度    )データ (    0 ～ 4096)
	* fpip->ycp_edit[n].cb	画素(色差(青))データ (-2048 ～ 2048)
	* fpip->ycp_edit[n].cr	画素(色差(赤))データ (-2048 ～ 2048)
	*
	*   画素データは範囲外に出ていることがあります。
	*   また範囲内に収めなくてもかまいません。
	*
	* 画像サイズを変えたいときは fpip->w や fpip->h を変えます。
	*
	* テンポラリ領域に処理した画像を格納したいときは
	* fpip->ycp_edit と fpip->ycp_temp を入れ替えます。
	*/
	// 計算しない場合はデフォルトのタイトルに直す
	if((fp->track[kTrackNoise] == 0) && (fp->track[kTrackScale] == 0)){
		if(!fp->exfunc->is_saving(fpip->editp)) SetWindowText(fp->hwnd, _T(kSoftName));
		return TRUE;
	}
	if(!fp->exfunc->is_saving(fpip->editp)){
		SetWindowText(fp->hwnd, _T("waifu2x-cpu(処理中...)"));
		start = std::chrono::system_clock::now();
	}

	// 設定に応じてモデルデータを選択し、処理を行う
	if(fp->track[kTrackNoise] > 0){
		// ノイズ除去する場合
		if(fp->check[0] == 0){
			SetFilter(fpip, fp->track[kTrackNoise] - 1, fp->track[kTrackThread]);
		} else{
			SetFilterWithBlocking(fpip, fp->track[kTrackNoise] - 1, fp->track[kTrackThread], fp->track[kTrackBlock]);
		}	
	}
	if(fp->track[kTrackScale] > 0){
		// 2x倍にスケールする場合
		//まず最近傍法で拡大する
		for(auto y = 0; y < fpip->h; y++) {
			for(auto k = 0; k < 2; ++k){
				auto ycp1 = fpip->ycp_edit + y           * fpip->max_w;
				auto ycp2 = fpip->ycp_temp + (y * 2 + k) * fpip->max_w;
				for(auto x = 0; x < fpip->w; x++) {
					ycp2->y = ycp1->y;
					ycp2->cb = ycp1->cb;
					ycp2->cr = ycp1->cr;
					ycp2[1].y = ycp1->y;
					ycp2[1].cb = ycp1->cb;
					ycp2[1].cr = ycp1->cr;
					ycp2 += 2;
					ycp1++;
				}
			}
		}
		fpip->w *= 2;
		fpip->h *= 2;
		auto ycp = fpip->ycp_edit;
		fpip->ycp_edit = fpip->ycp_temp;
		fpip->ycp_temp = ycp;
		//そしてフィルタ処理する
		if(fp->check[0] == 0){
			SetFilter(fpip, kModelScale2x, fp->track[kTrackThread]);
		} else{
			SetFilterWithBlocking(fpip, kModelScale2x, fp->track[kTrackThread], fp->track[kTrackBlock]);
		}
	}

	// 演算時間をタイトルバーに表示する
	if(!fp->exfunc->is_saving(fpip->editp)){
		auto end = std::chrono::system_clock::now();
		auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
		std::stringstream title_bar;
		title_bar << "waifu2x-cpu(" << ms << "ms)";
		SetWindowText(fp->hwnd, _T(title_bar.str().c_str()));
	}
	return TRUE;
}

/* フィルタ処理(ブロッキングなし) */
void SetFilter(FILTER_PROC_INFO *fpip, const int mode_, const int thread_){
	int steps_size = g_models[mode_].steps.size();
	//x_simd_stepを付け足したのは、右端が何故かぼやけるのを修正するため。
	//冷静に考えなくてもバッドノウハウだけど仕方ないね……
	int x_size = fpip->w + steps_size * 2 + x_simd_step;
	int y_size = fpip->h + steps_size * 2;
	// Y成分を[0,1]に正規化する
	vector< vector < vector<float> > >input_picture_y(kMaxInput, vector < vector<float> >(y_size, vector<float>(x_size)));
	for(auto y = 0; y < fpip->h; y++) {
		auto ycp = fpip->ycp_edit + y * fpip->max_w;
		for(auto x = 0; x < fpip->w; x++) {
			float normalized_y = 1.0f * ycp->y / 4096.0f;
			if(normalized_y < 0.0f) normalized_y = 0.0f;
			if(normalized_y > 1.0f) normalized_y = 1.0f;
			input_picture_y[0][y + steps_size][x + steps_size] = normalized_y;
			ycp++;
		}
	}
	// 辺の部分を拡張する
	for(auto y = 0; y < steps_size; ++y){	//左上
		for(auto x = 0; x < steps_size; ++x){
			input_picture_y[0][y][x] = input_picture_y[0][steps_size][steps_size];
		}
	}
	for(auto y = 0; y < steps_size; ++y){
		for(auto x = steps_size; x < fpip->w + steps_size; ++x){	//上
			input_picture_y[0][y][x] = input_picture_y[0][steps_size][x];
		}
	}
	for(auto y = 0; y < steps_size; ++y){	//右上
		for(auto x = fpip->w + steps_size; x < x_size; ++x){
			input_picture_y[0][y][x] = input_picture_y[0][steps_size][fpip->w + steps_size - 1];
		}
	}
	for(auto y = steps_size; y < fpip->h + steps_size; ++y){	//右
		for(auto x = fpip->w + steps_size; x < x_size; ++x){
			input_picture_y[0][y][x] = input_picture_y[0][y][fpip->w + steps_size - 1];
		}
	}
	for(auto y = fpip->h + steps_size; y < y_size; ++y){	//右下
		for(auto x = fpip->w + steps_size; x < x_size; ++x){
			input_picture_y[0][y][x] = input_picture_y[0][fpip->h + steps_size - 1][fpip->w + steps_size - 1];
		}
	}
	for(auto y = fpip->h + steps_size; y < y_size; ++y){	//下
		for(auto x = steps_size; x < fpip->w + steps_size; ++x){
			input_picture_y[0][y][x] = input_picture_y[0][fpip->h + steps_size - 1][x];
		}
	}
	for(auto y = fpip->h + steps_size; y < y_size; ++y) {	//左下
		for(auto x = 0; x < steps_size; ++x){
			input_picture_y[0][y][x] = input_picture_y[0][fpip->h + steps_size - 1][steps_size];
		}
	}
	for(auto y = steps_size; y < fpip->h + steps_size; ++y){//左
		for(auto x = 0; x < steps_size; ++x){
			input_picture_y[0][y][x] = input_picture_y[0][y][steps_size];
		}
	}
	// メインループ
	vector< vector < vector<float> > >output_picture_y(kMaxOutput, vector < vector<float> >(y_size, vector<float>(x_size)));
	x_size -= 2;
	y_size -= 2;
	for(auto s = 0; s < steps_size; ++s){
		Step* step_data = &g_models[mode_].steps[s];
		auto input_plane_size = step_data->input_plane_size;
		auto output_plane_size = step_data->output_plane_size;
		for(auto o = 0; o < output_plane_size; ++o){
			for(auto y = 0; y < y_size; ++y){
				for(auto x = 0; x < x_size; ++x){
					output_picture_y[o][y][x] = 0.0f;
				}
			}
		}
		// 畳み込み演算
		int x_size_ = x_size / x_simd_step * x_simd_step;
		#pragma omp parallel for num_threads(thread_)
		for(auto o = 0; o < output_plane_size; ++o){
			// 3x3のフィルタ処理
			for(auto i = 0; i < input_plane_size; ++i){
				// 割り切れる部分は纏めて処理してしまう
				for(auto y = 0; y < y_size; ++y){
					for(auto x = 0; x < x_size_; x += x_simd_step){
						__m128 input_simd[kFilterSize];
						__m128 *w1 = step_data->weight1[o][i];
						for(auto h = 0; h < kHeightSize; ++h){
							for(auto w = 0; w < kWidthSize; ++w){
								input_simd[h * kWidthSize + w] = _mm_loadu_ps(&input_picture_y[i][y + h][x + w]);
							}
						}
						__m128 sum_simd = _mm_setzero_ps();
						for(auto k = 0; k < kFilterSize; ++k){
							sum_simd = _mm_add_ps(sum_simd, _mm_mul_ps(w1[k], input_simd[k]));
						}
						__declspec(align(16)) float sum[x_simd_step];
						_mm_store_ps(sum, sum_simd);
						for(auto k = 0; k < x_simd_step; ++k){
							output_picture_y[o][y][x + k] += sum[k];
						}
					}
				}
				// 残りの部分はそこそこのSIMD化で乗り切る
				__m128 *w2 = step_data->weight2[o][i];
				for(auto y = 0; y < y_size; ++y){
					for(auto x = x_size_; x < x_size; ++x){
						__m128 input_simd0 = _mm_set_ps(input_picture_y[i][y + 0][x + 0], input_picture_y[i][y + 0][x + 1], input_picture_y[i][y + 0][x + 2], input_picture_y[i][y + 1][x + 0]);
						__m128 input_simd1 = _mm_set_ps(input_picture_y[i][y + 1][x + 1], input_picture_y[i][y + 1][x + 2], input_picture_y[i][y + 2][x + 0], input_picture_y[i][y + 2][x + 1]);
						__m128 input_simd2 = _mm_set_ps(input_picture_y[i][y + 2][x + 2], 0.0f, 0.0f, 0.0f);
						__m128 sum_simd = _mm_mul_ps(w2[0], input_simd0);
						sum_simd = _mm_add_ps(sum_simd, _mm_mul_ps(w2[1], input_simd1));
						sum_simd = _mm_add_ps(sum_simd, _mm_mul_ps(w2[2], input_simd2));
						// ここから
						__m128 tmp = sum_simd;
						sum_simd = _mm_shuffle_ps(sum_simd, tmp, _MM_SHUFFLE(1, 0, 3, 2));
						sum_simd = _mm_add_ps(sum_simd, tmp);
						tmp = sum_simd;
						sum_simd = _mm_shuffle_ps(sum_simd, tmp, _MM_SHUFFLE(2, 3, 0, 1));
						sum_simd = _mm_add_ps(sum_simd, tmp);
						// ここまでが水平演算
						__declspec(align(16)) float sum;
						_mm_store_ss(&sum, sum_simd);
						output_picture_y[o][y][x] += sum;
					}
				}
			}
			// バイアスを掛ける
			for(auto y = 0; y < y_size; ++y){
				for(auto x = 0; x < x_size; ++x){
					output_picture_y[o][y][x] += step_data->bias[o];
				}
			}
		}
		// 次ステップのために調整する
		for(auto o = 0; o < output_plane_size; ++o){
			for(auto y = 0; y < y_size; ++y){
				for(auto x = 0; x < x_size; ++x){
					input_picture_y[o][y][x] = output_picture_y[o][y][x];
					if(input_picture_y[o][y][x] < 0.0f) input_picture_y[o][y][x] *= 0.1f;
				}
			}
		}
		x_size -= 2;
		y_size -= 2;
	}

	// 結果を戻す
	for(auto y = 0; y < fpip->h; y++) {
		auto ycp = fpip->ycp_edit + y * fpip->max_w;
		for(auto x = 0; x < fpip->w; x++) {
			ycp->y = static_cast<short>(round(input_picture_y[0][y][x] * 4096));
			ycp++;
		}
	}
}

/* フィルタ処理(ブロッキングあり) */
void SetFilterWithBlocking(FILTER_PROC_INFO *fpip, const int mode_, const int thread_, const int block_){
	int steps_size = g_models[mode_].steps.size();
	// 境界線処理を正常にするためのマージンサイズを決定する
	int margin_size = steps_size + 1;
	while((block_ + margin_size) % x_simd_step != 0){
		++margin_size;
	}
	// パディング処理をブロッキング向けにするため、予めパディングしておいたのを用意する
	// (省メモリに微妙に反するが、境界線上がおかしくなるから仕方ないね……)
	int x_size_big = fpip->w + steps_size * 2 + margin_size;
	int y_size_big = fpip->h + steps_size * 2 + margin_size;
	//Y成分を[0,1]に正規化する
	vector < vector<float> >input_picture_y_big(y_size_big, vector<float>(x_size_big));
	for(auto y = 0; y < fpip->h; y++) {
		auto ycp = fpip->ycp_edit + y * fpip->max_w;
		for(auto x = 0; x < fpip->w; x++) {
			float normalized_y = 1.0f * ycp->y / 4096.0f;
			if(normalized_y < 0.0f) normalized_y = 0.0f;
			if(normalized_y > 1.0f) normalized_y = 1.0f;
			input_picture_y_big[y + steps_size][x + steps_size] = normalized_y;
			ycp++;
		}
	}
	//辺の部分を拡張する
	for(auto y = 0; y < steps_size; ++y){	//左上
		for(auto x = 0; x < steps_size; ++x){
			input_picture_y_big[y][x] = input_picture_y_big[steps_size][steps_size];
		}
	}
	for(auto y = 0; y < steps_size; ++y){
		for(auto x = steps_size; x < fpip->w + steps_size; ++x){	//上
			input_picture_y_big[y][x] = input_picture_y_big[steps_size][x];
		}
	}
	for(auto y = 0; y < steps_size; ++y){	//右上
		for(auto x = fpip->w + steps_size; x < x_size_big; ++x){
			input_picture_y_big[y][x] = input_picture_y_big[steps_size][fpip->w + steps_size - 1];
		}
	}
	for(auto y = steps_size; y < fpip->h + steps_size; ++y){	//右
		for(auto x = fpip->w + steps_size; x < x_size_big; ++x){
			input_picture_y_big[y][x] = input_picture_y_big[y][fpip->w + steps_size - 1];
		}
	}
	for(auto y = fpip->h + steps_size; y < y_size_big; ++y){	//右下
		for(auto x = fpip->w + steps_size; x < x_size_big; ++x){
			input_picture_y_big[y][x] = input_picture_y_big[fpip->h + steps_size - 1][fpip->w + steps_size - 1];
		}
	}
	for(auto y = fpip->h + steps_size; y < y_size_big; ++y){	//下
		for(auto x = steps_size; x < fpip->w + steps_size; ++x){
			input_picture_y_big[y][x] = input_picture_y_big[fpip->h + steps_size - 1][x];
		}
	}
	for(auto y = fpip->h + steps_size; y < y_size_big; ++y) {	//左下
		for(auto x = 0; x < steps_size; ++x){
			input_picture_y_big[y][x] = input_picture_y_big[fpip->h + steps_size - 1][steps_size];
		}
	}
	for(auto y = steps_size; y < fpip->h + steps_size; ++y){//左
		for(auto x = 0; x < steps_size; ++x){
			input_picture_y_big[y][x] = input_picture_y_big[y][steps_size];
		}
	}
	// ブロックに分割して処理する
	int block_num_x = fpip->w / block_;
	int block_num_y = fpip->h / block_;
	if(fpip->w % block_ != 0) ++block_num_x;
	if(fpip->h % block_ != 0) ++block_num_y;
	int y_size_base = block_, y_size = y_size_base + steps_size * 2;
	for(auto block_pos_y = 0; block_pos_y < block_num_y; ++block_pos_y){
		if(block_pos_y == block_num_y - 1){
			y_size_base = fpip->h - block_pos_y * block_;
			y_size = y_size_base + steps_size * 2;
		}
		int x_size_base = block_, x_size = x_size_base + steps_size * 2;
		for(auto block_pos_x = 0; block_pos_x < block_num_x; ++block_pos_x){
			if(block_pos_x == block_num_x - 1){
				x_size_base = fpip->w - block_pos_x * block_;
				x_size = x_size_base + steps_size * 2;
			}
			/* 入力部分 */
			x_size += margin_size; y_size += margin_size;
			vector< vector < vector<float> > >input_picture_y(kMaxInput, vector < vector<float> >(y_size, vector<float>(x_size)));
			for(auto y = 0; y < y_size; y++) {
				for(auto x = 0; x < x_size; x++) {
					input_picture_y[0][y][x] = input_picture_y_big[y + block_pos_y * block_][x + block_pos_x * block_];
				}
			}
			x_size_base += margin_size; y_size_base += margin_size;
			/* 演算部分 */
			// メインループ
			vector< vector < vector<float> > >output_picture_y(kMaxOutput, vector < vector<float> >(y_size, vector<float>(x_size)));
			auto x_size_ = x_size - 2;
			auto y_size_ = y_size - 2;
			for(auto s = 0; s < steps_size; ++s){
				Step* step_data = &g_models[mode_].steps[s];
				auto input_plane_size = step_data->input_plane_size;
				auto output_plane_size = step_data->output_plane_size;
				for(auto o = 0; o < output_plane_size; ++o){
					for(auto y = 0; y < y_size_; ++y){
						for(auto x = 0; x < x_size_; ++x){
							output_picture_y[o][y][x] = 0.0f;
						}
					}
				}
				// 畳み込み演算
				int x_size__ = x_size_ / x_simd_step * x_simd_step;
				#pragma omp parallel for num_threads(thread_)
				for(auto o = 0; o < output_plane_size; ++o){
					// 3x3のフィルタ処理
					for(auto i = 0; i < input_plane_size; ++i){
						// 割り切れる部分は纏めて処理してしまう
						for(auto y = 0; y < y_size_; ++y){
							for(auto x = 0; x < x_size__; x += x_simd_step){
								__m128 input_simd[kFilterSize];
								__m128 *w1 = step_data->weight1[o][i];
								for(auto h = 0; h < kHeightSize; ++h){
									for(auto w = 0; w < kWidthSize; ++w){
										input_simd[h * kWidthSize + w] = _mm_loadu_ps(&input_picture_y[i][y + h][x + w]);
									}
								}
								__m128 sum_simd = _mm_setzero_ps();
								for(auto k = 0; k < kFilterSize; ++k){
									sum_simd = _mm_add_ps(sum_simd, _mm_mul_ps(w1[k], input_simd[k]));
								}
								__declspec(align(16)) float sum[x_simd_step];
								_mm_store_ps(sum, sum_simd);
								for(auto k = 0; k < x_simd_step; ++k){
									output_picture_y[o][y][x + k] += sum[k];
								}
							}
						}
						// 残りの部分はそこそこのSIMD化で乗り切る
						__m128 *w2 = step_data->weight2[o][i];
						for(auto y = 0; y < y_size_; ++y){
							for(auto x = x_size__; x < x_size; ++x){
								__m128 input_simd0 = _mm_set_ps(input_picture_y[i][y + 0][x + 0], input_picture_y[i][y + 0][x + 1], input_picture_y[i][y + 0][x + 2], input_picture_y[i][y + 1][x + 0]);
								__m128 input_simd1 = _mm_set_ps(input_picture_y[i][y + 1][x + 1], input_picture_y[i][y + 1][x + 2], input_picture_y[i][y + 2][x + 0], input_picture_y[i][y + 2][x + 1]);
								__m128 input_simd2 = _mm_set_ps(input_picture_y[i][y + 2][x + 2], 0.0f, 0.0f, 0.0f);
								__m128 sum_simd = _mm_mul_ps(w2[0], input_simd0);
								sum_simd = _mm_add_ps(sum_simd, _mm_mul_ps(w2[1], input_simd1));
								sum_simd = _mm_add_ps(sum_simd, _mm_mul_ps(w2[2], input_simd2));
								// ここから
								__m128 tmp = sum_simd;
								sum_simd = _mm_shuffle_ps(sum_simd, tmp, _MM_SHUFFLE(1, 0, 3, 2));
								sum_simd = _mm_add_ps(sum_simd, tmp);
								tmp = sum_simd;
								sum_simd = _mm_shuffle_ps(sum_simd, tmp, _MM_SHUFFLE(2, 3, 0, 1));
								sum_simd = _mm_add_ps(sum_simd, tmp);
								// ここまでが水平演算
								__declspec(align(16)) float sum;
								_mm_store_ss(&sum, sum_simd);
								output_picture_y[o][y][x] += sum;
							}
						}
					}
					// バイアスを掛ける
					for(auto y = 0; y < y_size_; ++y){
						for(auto x = 0; x < x_size_; ++x){
							output_picture_y[o][y][x] += step_data->bias[o];
						}
					}
				}
				// 次ステップのために調整する
				for(auto o = 0; o < output_plane_size; ++o){
					for(auto y = 0; y < y_size_; ++y){
						for(auto x = 0; x < x_size_; ++x){
							input_picture_y[o][y][x] = output_picture_y[o][y][x];
							if(input_picture_y[o][y][x] < 0.0f) input_picture_y[o][y][x] *= 0.1f;
						}
					}
				}
				x_size_ -= 2;
				y_size_ -= 2;
			}
			/* 出力部分 */
			x_size_base -= margin_size; y_size_base -= margin_size;
			for(auto y = 0; y < y_size_base; y++) {
				int y_ = block_pos_y * block_ + y;
				auto ycp = fpip->ycp_edit + y_ * fpip->max_w + block_pos_x * block_;
				for(auto x = 0; x < x_size_base; x++) {
					ycp->y = static_cast<short>(round(input_picture_y[0][y][x] * 4096));
					ycp++;
				}
			}
			x_size -= margin_size; y_size -= margin_size;
		}
	}
}