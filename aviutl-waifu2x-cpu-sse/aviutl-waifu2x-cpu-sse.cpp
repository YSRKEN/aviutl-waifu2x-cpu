/* waifu2x-cpu Ver.1.1 by YSR */

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

const int kTracks = 3;									//トラックバーの数
TCHAR	*track_name[] = {"thread", "noise", "scale"};	//トラックバーの名前
int		track_default[] = {1, 0, 0};		//トラックバーの初期値
int		track_s[] = {1, 0, 0};		//トラックバーの下限値
int		track_e[] = {32, 2, 1};		//トラックバーの上限値

/* テンプレ */
FILTER_DLL filter = {
	FILTER_FLAG_EX_INFORMATION,
	0, 0,						//設定ウインドウのサイズ (FILTER_FLAG_WINDOW_SIZEが立っている時に有効)
	"waifu2x-cpu[SSE]",			//フィルタの名前
	kTracks,					//トラックバーの数 (0なら名前初期値等もNULLでよい)
	track_name,					//トラックバーの名前郡へのポインタ
	track_default,				//トラックバーの初期値郡へのポインタ
	track_s, track_e,			//トラックバーの数値の下限上限 (NULLなら全て0～256)
	NULL,						//チェックボックスの数 (0なら名前初期値等もNULLでよい)
	NULL,						//チェックボックスの名前郡へのポインタ
	NULL,						//チェックボックスの初期値郡へのポインタ
	func_proc,					//フィルタ処理関数へのポインタ (NULLなら呼ばれません)
	func_init,					//開始時に呼ばれる関数へのポインタ (NULLなら呼ばれません)
	NULL,						//終了時に呼ばれる関数へのポインタ (NULLなら呼ばれません)
	NULL,						//設定が変更されたときに呼ばれる関数へのポインタ (NULLなら呼ばれません)
	NULL,						//設定ウィンドウにウィンドウメッセージが来た時に呼ばれる関数へのポインタ (NULLなら呼ばれません)
	NULL, NULL,					//システムで使いますので使用しないでください
	NULL,						//拡張データ領域へのポインタ (FILTER_FLAG_EX_DATAが立っている時に有効)
	NULL,						//拡張データサイズ (FILTER_FLAG_EX_DATAが立っている時に有効)
	"waifu2x-cpu version 1.1 by YSR",
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
enum kTrackBar { kTrackThread, kTrackNoise, kTrackScale };
enum kModelKind { kModelDenoise1, kModelDenoise2, kModelScale2x, kModels };
const int kMaxInput = 128;
const int kMaxOutput = 128;
const int kWidthSize = 3;
const int kHeightSize = 3;

/* クラス定義 */
// 1ステップにおけるデータ
struct Step{
	int output_plane_size;
	int input_plane_size;
	__declspec(align(16)) float weight[kMaxInput][kMaxOutput][sizeof(__m128) * 3];
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
					for(auto wh = 0; wh < kWidthSize * kHeightSize; ++wh){
						fin.read(reinterpret_cast<char*>(&steps[s].weight[o][i][wh]), sizeof(float));
					}
					for(auto k = kWidthSize * kHeightSize; k < sizeof(__m128) * 3; ++k){
						steps[s].weight[o][i][k] = 0.0f;
					}
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
		if(!fp->exfunc->is_saving(fpip->editp)) SetWindowText(fp->hwnd, _T("waifu2x-cpu"));
		return TRUE;
	}
	if(!fp->exfunc->is_saving(fpip->editp)){
		SetWindowText(fp->hwnd, _T("waifu2x-cpu(処理中...)"));
		start = std::chrono::system_clock::now();
	}

	// 設定に応じてモデルデータを選択し、処理を行う
	if(fp->track[kTrackNoise] > 0){
		// ノイズ除去する場合
		SetFilter(fpip, fp->track[kTrackNoise] - 1, fp->track[kTrackThread]);
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
		SetFilter(fpip, kModelScale2x, fp->track[kTrackThread]);
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

void SetFilter(FILTER_PROC_INFO *fpip, const int mode_, const int thread_){
	int steps_size = g_models[mode_].steps.size();
	// Y成分を[0,1]に正規化する
	vector< vector < vector<float> > >input_picture_y(kMaxInput, vector < vector<float> >(fpip->w + steps_size * 2, vector<float>(fpip->h + steps_size * 2)));
	for(auto y = 0; y < fpip->h; y++) {
		auto ycp = fpip->ycp_edit + y * fpip->max_w;
		for(auto x = 0; x < fpip->w; x++) {
			float normalized_y = 1.0f * ycp->y / 4096.0f;
			if(normalized_y < 0.0f) normalized_y = 0.0f;
			if(normalized_y > 1.0f) normalized_y = 1.0f;
			input_picture_y[0][x + steps_size][y + steps_size] = normalized_y;
			ycp++;
		}
	}
	// 辺の部分を拡張する
	for(auto y = 0; y < steps_size; ++y){	//左上
		for(auto x = 0; x < steps_size; ++x){
			input_picture_y[0][x][y] = input_picture_y[0][steps_size][steps_size];
		}
	}
	for(auto y = 0; y < steps_size; ++y){
		for(auto x = steps_size; x < fpip->w + steps_size; ++x){	//上
			input_picture_y[0][x][y] = input_picture_y[0][x][steps_size];
		}
	}
	for(auto y = 0; y < steps_size; ++y){	//右上
		for(auto x = fpip->w + steps_size; x < fpip->w + steps_size * 2; ++x){
			input_picture_y[0][x][y] = input_picture_y[0][fpip->w + steps_size - 1][steps_size];
		}
	}
	for(auto y = steps_size; y < fpip->h + steps_size; ++y){	//右
		for(auto x = fpip->w + steps_size; x < fpip->w + steps_size * 2; ++x){
			input_picture_y[0][x][y] = input_picture_y[0][fpip->w + steps_size - 1][y];
		}
	}
	for(auto y = fpip->h + steps_size; y < fpip->h + steps_size * 2; ++y){	//右下
		for(auto x = fpip->w + steps_size; x < fpip->w + steps_size * 2; ++x){
			input_picture_y[0][x][y] = input_picture_y[0][fpip->w + steps_size - 1][fpip->h + steps_size - 1];
		}
	}
	for(auto y = fpip->h + steps_size; y < fpip->h + steps_size * 2; ++y){	//下
		for(auto x = steps_size; x < fpip->w + steps_size; ++x){
			input_picture_y[0][x][y] = input_picture_y[0][x][fpip->h + steps_size - 1];
		}
	}
	for(auto y = fpip->h + steps_size; y < fpip->h + steps_size * 2; ++y) {	//左下
		for(auto x = 0; x < steps_size; ++x){
			input_picture_y[0][x][y] = input_picture_y[0][steps_size][fpip->h + steps_size - 1];
		}
	}
	for(auto y = steps_size; y < fpip->h + steps_size; ++y){//左
		for(auto x = 0; x < steps_size; ++x){
			input_picture_y[0][x][y] = input_picture_y[0][steps_size][y];
		}
	}
	// メインループ
	vector< vector < vector<float> > >output_picture_y(kMaxOutput, vector < vector<float> >(fpip->w + steps_size * 2, vector<float>(fpip->h + steps_size * 2)));
	int padding = (steps_size - 1) * 2;
	int x_size = fpip->w + padding, y_size = fpip->h + padding;
	Step* step_data;
	int x_size_ = x_size, y_size_ = y_size;
	for(auto s = 0; s < steps_size; ++s){
		step_data = &g_models[mode_].steps[s];
		auto input_plane_size = step_data->input_plane_size;
		auto output_plane_size = step_data->output_plane_size;
		for(auto o = 0; o < output_plane_size; ++o){
			for(auto y = 0; y < y_size; ++y){
				for(auto x = 0; x < x_size; ++x){
					output_picture_y[o][x][y] = 0.0f;
				}
			}
		}
		// 畳み込み演算
		#pragma omp parallel for num_threads(thread_)
		for(auto o = 0; o < output_plane_size; ++o){
			// 3x3のフィルタ処理
			for(auto i = 0; i < input_plane_size; ++i){
				__m128 weight_simd0 = _mm_load_ps(&step_data->weight[o][i][0]);
				__m128 weight_simd1 = _mm_load_ps(&step_data->weight[o][i][4]);
				__m128 weight_simd2 = _mm_load_ps(&step_data->weight[o][i][8]);
				/*__m128 weight_simd0 = _mm_set_ps(step_data->weight[o][i][0], step_data->weight[o][i][1], step_data->weight[o][i][2], step_data->weight[o][i][3]);
				__m128 weight_simd1 = _mm_set_ps(step_data->weight[o][i][4], step_data->weight[o][i][5], step_data->weight[o][i][6], step_data->weight[o][i][7]);
				__m128 weight_simd2 = _mm_set_ps(step_data->weight[o][i][8], 0.0f, 0.0f, 0.0f);*/
				for(auto y = 0; y < y_size; ++y){
					for(auto x = 0; x < x_size; ++x){
						__m128 input_simd0 = _mm_set_ps(input_picture_y[i][x + 1][y + 0], input_picture_y[i][x + 0][y + 2], input_picture_y[i][x + 0][y + 1], input_picture_y[i][x + 0][y + 0]);
						__m128 input_simd1 = _mm_set_ps(input_picture_y[i][x + 2][y + 1], input_picture_y[i][x + 2][y + 0], input_picture_y[i][x + 1][y + 2], input_picture_y[i][x + 1][y + 1]);
						__m128 input_simd2 = _mm_set_ps(0.0f, 0.0f, 0.0f, input_picture_y[i][x + 2][y + 2]);
						/*__m128 input_simd0 = _mm_set_ps(input_picture_y[i][x + 0][y + 0], input_picture_y[i][x + 0][y + 1], input_picture_y[i][x + 0][y + 2], input_picture_y[i][x + 1][y + 0]);
						__m128 input_simd1 = _mm_set_ps(input_picture_y[i][x + 1][y + 1], input_picture_y[i][x + 1][y + 2], input_picture_y[i][x + 2][y + 0], input_picture_y[i][x + 2][y + 1]);
						__m128 input_simd2 = _mm_set_ps(input_picture_y[i][x + 2][y + 2], 0.0f, 0.0f, 0.0f);*/
						__m128 sum_simd = _mm_mul_ps(weight_simd0, input_simd0);
						sum_simd = _mm_add_ps(sum_simd, _mm_mul_ps(weight_simd1, input_simd1));
						sum_simd = _mm_add_ps(sum_simd, _mm_mul_ps(weight_simd2, input_simd2));
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
						output_picture_y[o][x][y] += sum;
					}
				}
			}
			// バイアスを掛ける
			for(auto y = 0; y < y_size; ++y){
				for(auto x = 0; x < x_size; ++x){
					output_picture_y[o][x][y] += step_data->bias[o];
				}
			}
		}
		// 次ステップのために調整する
		for(auto o = 0; o < output_plane_size; ++o){
			for(auto y = 0; y < y_size; ++y){
				for(auto x = 0; x < x_size; ++x){
					input_picture_y[o][x][y] = output_picture_y[o][x][y];
					if(input_picture_y[o][x][y] < 0.0f) input_picture_y[o][x][y] *= 0.1f;
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
			ycp->y = static_cast<short>(round(input_picture_y[0][x][y] * 4096));
			ycp++;
		}
	}
}
