/* waifu2x-cpu Ver.1.2 by YSR */

/* �v���v���Z�b�T */
// STL
#include <chrono>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
// Windows�ˑ�
#include <tchar.h>
#include <windows.h>
// SIMD
#include <xmmintrin.h>
// AviUtl�֌W
#include "filter.h"
#define kSoftName "waifu2x-cpu[SSE]"

const int kTracks = 4;											//�g���b�N�o�[�̐�
TCHAR	*track_name[] = {"thread", "noise", "scale", "block"};	//�g���b�N�o�[�̖��O
int		track_default[] = {1, 0, 0, 32};						//�g���b�N�o�[�̏����l
int		track_s[] = {1, 0, 0, 32};								//�g���b�N�o�[�̉����l
int		track_e[] = {32, 2, 1, 256};							//�g���b�N�o�[�̏���l

const int kChecks = 1;						//�`�F�b�N�{�b�N�X�̐�
TCHAR	*check_name[] = {"use blocking"};	//�`�F�b�N�{�b�N�X�̖��O
int		check_default[] = {0};				//�`�F�b�N�{�b�N�X�̏����l (�l��0��1)

/* �e���v�� */
FILTER_DLL filter = {
	FILTER_FLAG_EX_INFORMATION,
	0, 0,						//�ݒ�E�C���h�E�̃T�C�Y (FILTER_FLAG_WINDOW_SIZE�������Ă��鎞�ɗL��)
	kSoftName,					//�t�B���^�̖��O
	kTracks,					//�g���b�N�o�[�̐� (0�Ȃ疼�O�����l����NULL�ł悢)
	track_name,					//�g���b�N�o�[�̖��O�S�ւ̃|�C���^
	track_default,				//�g���b�N�o�[�̏����l�S�ւ̃|�C���^
	track_s, track_e,			//�g���b�N�o�[�̐��l�̉������ (NULL�Ȃ�S��0�`256)
	kChecks,					//�`�F�b�N�{�b�N�X�̐� (0�Ȃ疼�O�����l����NULL�ł悢)
	check_name,					//�`�F�b�N�{�b�N�X�̖��O�S�ւ̃|�C���^
	check_default,				//�`�F�b�N�{�b�N�X�̏����l�S�ւ̃|�C���^
	func_proc,					//�t�B���^�����֐��ւ̃|�C���^ (NULL�Ȃ�Ă΂�܂���)
	func_init,					//�J�n���ɌĂ΂��֐��ւ̃|�C���^ (NULL�Ȃ�Ă΂�܂���)
	NULL,						//�I�����ɌĂ΂��֐��ւ̃|�C���^ (NULL�Ȃ�Ă΂�܂���)
	NULL,						//�ݒ肪�ύX���ꂽ�Ƃ��ɌĂ΂��֐��ւ̃|�C���^ (NULL�Ȃ�Ă΂�܂���)
	NULL,						//�ݒ�E�B���h�E�ɃE�B���h�E���b�Z�[�W���������ɌĂ΂��֐��ւ̃|�C���^ (NULL�Ȃ�Ă΂�܂���)
	NULL, NULL,					//�V�X�e���Ŏg���܂��̂Ŏg�p���Ȃ��ł�������
	NULL,						//�g���f�[�^�̈�ւ̃|�C���^ (FILTER_FLAG_EX_DATA�������Ă��鎞�ɗL��)
	NULL,						//�g���f�[�^�T�C�Y (FILTER_FLAG_EX_DATA�������Ă��鎞�ɗL��)
	"waifu2x-cpu version 1.2 by YSR",
	//�t�B���^���ւ̃|�C���^ (FILTER_FLAG_EX_INFORMATION�������Ă��鎞�ɗL��)
	NULL,						//�Z�[�u���J�n����钼�O�ɌĂ΂��֐��ւ̃|�C���^ (NULL�Ȃ�Ă΂�܂���)
	NULL,						//�Z�[�u���I���������O�ɌĂ΂��֐��ւ̃|�C���^ (NULL�Ȃ�Ă΂�܂���)
};

EXTERN_C FILTER_DLL __declspec(dllexport) * __stdcall GetFilterTable(void){ return &filter; }

/* using�錾 */
using std::stod;
using std::stoi;
using std::string;
using std::vector;

/* �萔�錾 */
enum kTrackBar { kTrackThread, kTrackNoise, kTrackScale, kTrackBlock};
enum kModelKind { kModelDenoise1, kModelDenoise2, kModelScale2x, kModels };
const int kMaxInput = 128;
const int kMaxOutput = 128;
const int kWidthSize = 3;
const int kHeightSize = 3;
const int kFilterSize = kWidthSize * kHeightSize;
const int x_simd_step = sizeof(__m128) / sizeof(float);

/* �N���X��` */
// 1�X�e�b�v�ɂ�����f�[�^
struct Step{
	int output_plane_size;
	int input_plane_size;
	__m128 weight1[kMaxOutput][kMaxInput][kFilterSize];
	__m128 weight2[kMaxOutput][kMaxInput][3];
	vector<float> bias;
};
// 1���f���ɂ�����f�[�^
struct Model{
	vector<Step> steps;
	// �������֐�
	void Init(const string filename){
		// �t�@�C�����J���Ȃ��ƃA�E�g
		std::ifstream fin(filename, std::ios_base::in | std::ios_base::binary);
		if(fin.fail()) throw filename;
		int steps_size;
		//fin >> steps_size;
		fin.read(reinterpret_cast<char*>(&steps_size), sizeof(int));
		steps.resize(steps_size);
		// input�����output
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
					/* �Y���̏��ԁF
					* W��H��WH�ƕ\���ꍇ�A���X��00 01 02 10 11 12 20 21 22�Ɗi�[���Ă���
					* �����X�D���00 10 20 01 11 21 02 12 22�ƃ�������ɕ��ׂ����̂ŁA
					* 0,1,2,3,4,5,6,7,8�Ԗڂ̃f�[�^��Y��0,3,6,1,4,7,2,5,8�Ԗڂɒu�����Ƃ�
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

/* �v���g�^�C�v�錾 */
void SetFilter(FILTER_PROC_INFO*, const int, const int);
void SetFilterWithBlocking(FILTER_PROC_INFO*, const int, const int, const int);

/* �O���[�o���ϐ��錾 */
/* ����Ȃ��Ƃ͂������Ȃ��������I
* �ł��A1�t���[������1�񃂃f���f�[�^��ǂݏo���Ȃ��
* �n���������Ƃ͂������Ȃ��������I
* �N�����ɂ����Ə�肢�Ώ��@�������Ă���c�c
*/
vector<Model> g_models(kModels);
auto start = std::chrono::system_clock::now();

/* �������֐� */
BOOL func_init(FILTER *fp){
	try{
		g_models[kModelDenoise1].Init(".\\plugins\\models\\noise1_model3.dat");
		g_models[kModelDenoise2].Init(".\\plugins\\models\\noise2_model3.dat");
		g_models[kModelScale2x].Init(".\\plugins\\models\\scale2.0x_model3.dat");
	}
	catch(string err_msg_){
		string err_msg = err_msg_ + "��ǂݍ��ލۂɃG���[���������܂����B";
		MessageBox(NULL, err_msg_.c_str(), "waifu2x-cpu", MB_OK);
		return FALSE;
	}
	return TRUE;
}

/* �����֐� */
BOOL func_proc(FILTER *fp, FILTER_PROC_INFO *fpip){
	/*
	* fp->track[n]			�g���b�N�o�[�̐��l
	* fp->check[n]			�`�F�b�N�{�b�N�X�̐��l
	* fpip->w 				���ۂ̉摜�̉���
	* fpip->h 				���ۂ̉摜�̏c��
	* fpip->max_w			�摜�̈�̉���
	* fpip->max_h			�摜�̈�̏c��
	* fpip->ycp_edit		�摜�̈�ւ̃|�C���^
	* fpip->ycp_temp		�e���|�����̈�ւ̃|�C���^
	* fpip->ycp_edit[n].y	��f(�P�x    )�f�[�^ (    0 �` 4096)
	* fpip->ycp_edit[n].cb	��f(�F��(��))�f�[�^ (-2048 �` 2048)
	* fpip->ycp_edit[n].cr	��f(�F��(��))�f�[�^ (-2048 �` 2048)
	*
	*   ��f�f�[�^�͔͈͊O�ɏo�Ă��邱�Ƃ�����܂��B
	*   �܂��͈͓��Ɏ��߂Ȃ��Ă����܂��܂���B
	*
	* �摜�T�C�Y��ς������Ƃ��� fpip->w �� fpip->h ��ς��܂��B
	*
	* �e���|�����̈�ɏ��������摜���i�[�������Ƃ���
	* fpip->ycp_edit �� fpip->ycp_temp �����ւ��܂��B
	*/
	// �v�Z���Ȃ��ꍇ�̓f�t�H���g�̃^�C�g���ɒ���
	if((fp->track[kTrackNoise] == 0) && (fp->track[kTrackScale] == 0)){
		if(!fp->exfunc->is_saving(fpip->editp)) SetWindowText(fp->hwnd, _T(kSoftName));
		return TRUE;
	}
	if(!fp->exfunc->is_saving(fpip->editp)){
		SetWindowText(fp->hwnd, _T("waifu2x-cpu(������...)"));
		start = std::chrono::system_clock::now();
	}

	// �ݒ�ɉ����ă��f���f�[�^��I�����A�������s��
	if(fp->track[kTrackNoise] > 0){
		// �m�C�Y��������ꍇ
		if(fp->check[0] == 0){
			SetFilter(fpip, fp->track[kTrackNoise] - 1, fp->track[kTrackThread]);
		} else{
			SetFilterWithBlocking(fpip, fp->track[kTrackNoise] - 1, fp->track[kTrackThread], fp->track[kTrackBlock]);
		}	
	}
	if(fp->track[kTrackScale] > 0){
		// 2x�{�ɃX�P�[������ꍇ
		//�܂��ŋߖT�@�Ŋg�傷��
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
		//�����ăt�B���^��������
		if(fp->check[0] == 0){
			SetFilter(fpip, kModelScale2x, fp->track[kTrackThread]);
		} else{
			SetFilterWithBlocking(fpip, kModelScale2x, fp->track[kTrackThread], fp->track[kTrackBlock]);
		}
	}

	// ���Z���Ԃ��^�C�g���o�[�ɕ\������
	if(!fp->exfunc->is_saving(fpip->editp)){
		auto end = std::chrono::system_clock::now();
		auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
		std::stringstream title_bar;
		title_bar << "waifu2x-cpu(" << ms << "ms)";
		SetWindowText(fp->hwnd, _T(title_bar.str().c_str()));
	}
	return TRUE;
}

/* �t�B���^����(�u���b�L���O�Ȃ�) */
void SetFilter(FILTER_PROC_INFO *fpip, const int mode_, const int thread_){
	int steps_size = g_models[mode_].steps.size();
	//x_simd_step��t���������̂́A�E�[�����̂��ڂ₯��̂��C�����邽�߁B
	//��Âɍl���Ȃ��Ă��o�b�h�m�E�n�E�����ǎd���Ȃ��ˁc�c
	int x_size = fpip->w + steps_size * 2 + x_simd_step;
	int y_size = fpip->h + steps_size * 2;
	// Y������[0,1]�ɐ��K������
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
	// �ӂ̕������g������
	for(auto y = 0; y < steps_size; ++y){	//����
		for(auto x = 0; x < steps_size; ++x){
			input_picture_y[0][y][x] = input_picture_y[0][steps_size][steps_size];
		}
	}
	for(auto y = 0; y < steps_size; ++y){
		for(auto x = steps_size; x < fpip->w + steps_size; ++x){	//��
			input_picture_y[0][y][x] = input_picture_y[0][steps_size][x];
		}
	}
	for(auto y = 0; y < steps_size; ++y){	//�E��
		for(auto x = fpip->w + steps_size; x < x_size; ++x){
			input_picture_y[0][y][x] = input_picture_y[0][steps_size][fpip->w + steps_size - 1];
		}
	}
	for(auto y = steps_size; y < fpip->h + steps_size; ++y){	//�E
		for(auto x = fpip->w + steps_size; x < x_size; ++x){
			input_picture_y[0][y][x] = input_picture_y[0][y][fpip->w + steps_size - 1];
		}
	}
	for(auto y = fpip->h + steps_size; y < y_size; ++y){	//�E��
		for(auto x = fpip->w + steps_size; x < x_size; ++x){
			input_picture_y[0][y][x] = input_picture_y[0][fpip->h + steps_size - 1][fpip->w + steps_size - 1];
		}
	}
	for(auto y = fpip->h + steps_size; y < y_size; ++y){	//��
		for(auto x = steps_size; x < fpip->w + steps_size; ++x){
			input_picture_y[0][y][x] = input_picture_y[0][fpip->h + steps_size - 1][x];
		}
	}
	for(auto y = fpip->h + steps_size; y < y_size; ++y) {	//����
		for(auto x = 0; x < steps_size; ++x){
			input_picture_y[0][y][x] = input_picture_y[0][fpip->h + steps_size - 1][steps_size];
		}
	}
	for(auto y = steps_size; y < fpip->h + steps_size; ++y){//��
		for(auto x = 0; x < steps_size; ++x){
			input_picture_y[0][y][x] = input_picture_y[0][y][steps_size];
		}
	}
	// ���C�����[�v
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
		// ��ݍ��݉��Z
		int x_size_ = x_size / x_simd_step * x_simd_step;
		#pragma omp parallel for num_threads(thread_)
		for(auto o = 0; o < output_plane_size; ++o){
			// 3x3�̃t�B���^����
			for(auto i = 0; i < input_plane_size; ++i){
				// ����؂�镔���͓Z�߂ď������Ă��܂�
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
				// �c��̕����͂���������SIMD���ŏ��؂�
				__m128 *w2 = step_data->weight2[o][i];
				for(auto y = 0; y < y_size; ++y){
					for(auto x = x_size_; x < x_size; ++x){
						__m128 input_simd0 = _mm_set_ps(input_picture_y[i][y + 0][x + 0], input_picture_y[i][y + 0][x + 1], input_picture_y[i][y + 0][x + 2], input_picture_y[i][y + 1][x + 0]);
						__m128 input_simd1 = _mm_set_ps(input_picture_y[i][y + 1][x + 1], input_picture_y[i][y + 1][x + 2], input_picture_y[i][y + 2][x + 0], input_picture_y[i][y + 2][x + 1]);
						__m128 input_simd2 = _mm_set_ps(input_picture_y[i][y + 2][x + 2], 0.0f, 0.0f, 0.0f);
						__m128 sum_simd = _mm_mul_ps(w2[0], input_simd0);
						sum_simd = _mm_add_ps(sum_simd, _mm_mul_ps(w2[1], input_simd1));
						sum_simd = _mm_add_ps(sum_simd, _mm_mul_ps(w2[2], input_simd2));
						// ��������
						__m128 tmp = sum_simd;
						sum_simd = _mm_shuffle_ps(sum_simd, tmp, _MM_SHUFFLE(1, 0, 3, 2));
						sum_simd = _mm_add_ps(sum_simd, tmp);
						tmp = sum_simd;
						sum_simd = _mm_shuffle_ps(sum_simd, tmp, _MM_SHUFFLE(2, 3, 0, 1));
						sum_simd = _mm_add_ps(sum_simd, tmp);
						// �����܂ł��������Z
						__declspec(align(16)) float sum;
						_mm_store_ss(&sum, sum_simd);
						output_picture_y[o][y][x] += sum;
					}
				}
			}
			// �o�C�A�X���|����
			for(auto y = 0; y < y_size; ++y){
				for(auto x = 0; x < x_size; ++x){
					output_picture_y[o][y][x] += step_data->bias[o];
				}
			}
		}
		// ���X�e�b�v�̂��߂ɒ�������
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

	// ���ʂ�߂�
	for(auto y = 0; y < fpip->h; y++) {
		auto ycp = fpip->ycp_edit + y * fpip->max_w;
		for(auto x = 0; x < fpip->w; x++) {
			ycp->y = static_cast<short>(round(input_picture_y[0][y][x] * 4096));
			ycp++;
		}
	}
}

/* �t�B���^����(�u���b�L���O����) */
void SetFilterWithBlocking(FILTER_PROC_INFO *fpip, const int mode_, const int thread_, const int block_){
	int steps_size = g_models[mode_].steps.size();
	// ���E�������𐳏�ɂ��邽�߂̃}�[�W���T�C�Y�����肷��
	int margin_size = steps_size + 1;
	while((block_ + margin_size) % x_simd_step != 0){
		++margin_size;
	}
	// �p�f�B���O�������u���b�L���O�����ɂ��邽�߁A�\�߃p�f�B���O���Ă������̂�p�ӂ���
	// (�ȃ������ɔ����ɔ����邪�A���E���オ���������Ȃ邩��d���Ȃ��ˁc�c)
	int x_size_big = fpip->w + steps_size * 2 + margin_size;
	int y_size_big = fpip->h + steps_size * 2 + margin_size;
	//Y������[0,1]�ɐ��K������
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
	//�ӂ̕������g������
	for(auto y = 0; y < steps_size; ++y){	//����
		for(auto x = 0; x < steps_size; ++x){
			input_picture_y_big[y][x] = input_picture_y_big[steps_size][steps_size];
		}
	}
	for(auto y = 0; y < steps_size; ++y){
		for(auto x = steps_size; x < fpip->w + steps_size; ++x){	//��
			input_picture_y_big[y][x] = input_picture_y_big[steps_size][x];
		}
	}
	for(auto y = 0; y < steps_size; ++y){	//�E��
		for(auto x = fpip->w + steps_size; x < x_size_big; ++x){
			input_picture_y_big[y][x] = input_picture_y_big[steps_size][fpip->w + steps_size - 1];
		}
	}
	for(auto y = steps_size; y < fpip->h + steps_size; ++y){	//�E
		for(auto x = fpip->w + steps_size; x < x_size_big; ++x){
			input_picture_y_big[y][x] = input_picture_y_big[y][fpip->w + steps_size - 1];
		}
	}
	for(auto y = fpip->h + steps_size; y < y_size_big; ++y){	//�E��
		for(auto x = fpip->w + steps_size; x < x_size_big; ++x){
			input_picture_y_big[y][x] = input_picture_y_big[fpip->h + steps_size - 1][fpip->w + steps_size - 1];
		}
	}
	for(auto y = fpip->h + steps_size; y < y_size_big; ++y){	//��
		for(auto x = steps_size; x < fpip->w + steps_size; ++x){
			input_picture_y_big[y][x] = input_picture_y_big[fpip->h + steps_size - 1][x];
		}
	}
	for(auto y = fpip->h + steps_size; y < y_size_big; ++y) {	//����
		for(auto x = 0; x < steps_size; ++x){
			input_picture_y_big[y][x] = input_picture_y_big[fpip->h + steps_size - 1][steps_size];
		}
	}
	for(auto y = steps_size; y < fpip->h + steps_size; ++y){//��
		for(auto x = 0; x < steps_size; ++x){
			input_picture_y_big[y][x] = input_picture_y_big[y][steps_size];
		}
	}
	// �u���b�N�ɕ������ď�������
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
			/* ���͕��� */
			x_size += margin_size; y_size += margin_size;
			vector< vector < vector<float> > >input_picture_y(kMaxInput, vector < vector<float> >(y_size, vector<float>(x_size)));
			for(auto y = 0; y < y_size; y++) {
				for(auto x = 0; x < x_size; x++) {
					input_picture_y[0][y][x] = input_picture_y_big[y + block_pos_y * block_][x + block_pos_x * block_];
				}
			}
			x_size_base += margin_size; y_size_base += margin_size;
			/* ���Z���� */
			// ���C�����[�v
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
				// ��ݍ��݉��Z
				int x_size__ = x_size_ / x_simd_step * x_simd_step;
				#pragma omp parallel for num_threads(thread_)
				for(auto o = 0; o < output_plane_size; ++o){
					// 3x3�̃t�B���^����
					for(auto i = 0; i < input_plane_size; ++i){
						// ����؂�镔���͓Z�߂ď������Ă��܂�
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
						// �c��̕����͂���������SIMD���ŏ��؂�
						__m128 *w2 = step_data->weight2[o][i];
						for(auto y = 0; y < y_size_; ++y){
							for(auto x = x_size__; x < x_size; ++x){
								__m128 input_simd0 = _mm_set_ps(input_picture_y[i][y + 0][x + 0], input_picture_y[i][y + 0][x + 1], input_picture_y[i][y + 0][x + 2], input_picture_y[i][y + 1][x + 0]);
								__m128 input_simd1 = _mm_set_ps(input_picture_y[i][y + 1][x + 1], input_picture_y[i][y + 1][x + 2], input_picture_y[i][y + 2][x + 0], input_picture_y[i][y + 2][x + 1]);
								__m128 input_simd2 = _mm_set_ps(input_picture_y[i][y + 2][x + 2], 0.0f, 0.0f, 0.0f);
								__m128 sum_simd = _mm_mul_ps(w2[0], input_simd0);
								sum_simd = _mm_add_ps(sum_simd, _mm_mul_ps(w2[1], input_simd1));
								sum_simd = _mm_add_ps(sum_simd, _mm_mul_ps(w2[2], input_simd2));
								// ��������
								__m128 tmp = sum_simd;
								sum_simd = _mm_shuffle_ps(sum_simd, tmp, _MM_SHUFFLE(1, 0, 3, 2));
								sum_simd = _mm_add_ps(sum_simd, tmp);
								tmp = sum_simd;
								sum_simd = _mm_shuffle_ps(sum_simd, tmp, _MM_SHUFFLE(2, 3, 0, 1));
								sum_simd = _mm_add_ps(sum_simd, tmp);
								// �����܂ł��������Z
								__declspec(align(16)) float sum;
								_mm_store_ss(&sum, sum_simd);
								output_picture_y[o][y][x] += sum;
							}
						}
					}
					// �o�C�A�X���|����
					for(auto y = 0; y < y_size_; ++y){
						for(auto x = 0; x < x_size_; ++x){
							output_picture_y[o][y][x] += step_data->bias[o];
						}
					}
				}
				// ���X�e�b�v�̂��߂ɒ�������
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
			/* �o�͕��� */
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