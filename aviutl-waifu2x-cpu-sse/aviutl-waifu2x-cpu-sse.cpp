/* waifu2x-cpu Ver.1.1 by YSR */

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

const int kTracks = 3;									//�g���b�N�o�[�̐�
TCHAR	*track_name[] = {"thread", "noise", "scale"};	//�g���b�N�o�[�̖��O
int		track_default[] = {1, 0, 0};		//�g���b�N�o�[�̏����l
int		track_s[] = {1, 0, 0};		//�g���b�N�o�[�̉����l
int		track_e[] = {32, 2, 1};		//�g���b�N�o�[�̏���l

/* �e���v�� */
FILTER_DLL filter = {
	FILTER_FLAG_EX_INFORMATION,
	0, 0,						//�ݒ�E�C���h�E�̃T�C�Y (FILTER_FLAG_WINDOW_SIZE�������Ă��鎞�ɗL��)
	"waifu2x-cpu[SSE]",			//�t�B���^�̖��O
	kTracks,					//�g���b�N�o�[�̐� (0�Ȃ疼�O�����l����NULL�ł悢)
	track_name,					//�g���b�N�o�[�̖��O�S�ւ̃|�C���^
	track_default,				//�g���b�N�o�[�̏����l�S�ւ̃|�C���^
	track_s, track_e,			//�g���b�N�o�[�̐��l�̉������ (NULL�Ȃ�S��0�`256)
	NULL,						//�`�F�b�N�{�b�N�X�̐� (0�Ȃ疼�O�����l����NULL�ł悢)
	NULL,						//�`�F�b�N�{�b�N�X�̖��O�S�ւ̃|�C���^
	NULL,						//�`�F�b�N�{�b�N�X�̏����l�S�ւ̃|�C���^
	func_proc,					//�t�B���^�����֐��ւ̃|�C���^ (NULL�Ȃ�Ă΂�܂���)
	func_init,					//�J�n���ɌĂ΂��֐��ւ̃|�C���^ (NULL�Ȃ�Ă΂�܂���)
	NULL,						//�I�����ɌĂ΂��֐��ւ̃|�C���^ (NULL�Ȃ�Ă΂�܂���)
	NULL,						//�ݒ肪�ύX���ꂽ�Ƃ��ɌĂ΂��֐��ւ̃|�C���^ (NULL�Ȃ�Ă΂�܂���)
	NULL,						//�ݒ�E�B���h�E�ɃE�B���h�E���b�Z�[�W���������ɌĂ΂��֐��ւ̃|�C���^ (NULL�Ȃ�Ă΂�܂���)
	NULL, NULL,					//�V�X�e���Ŏg���܂��̂Ŏg�p���Ȃ��ł�������
	NULL,						//�g���f�[�^�̈�ւ̃|�C���^ (FILTER_FLAG_EX_DATA�������Ă��鎞�ɗL��)
	NULL,						//�g���f�[�^�T�C�Y (FILTER_FLAG_EX_DATA�������Ă��鎞�ɗL��)
	"waifu2x-cpu version 1.1 by YSR",
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
enum kTrackBar { kTrackThread, kTrackNoise, kTrackScale };
enum kModelKind { kModelDenoise1, kModelDenoise2, kModelScale2x, kModels };
const int kMaxInput = 128;
const int kMaxOutput = 128;
const int kWidthSize = 3;
const int kHeightSize = 3;

/* �N���X��` */
// 1�X�e�b�v�ɂ�����f�[�^
struct Step{
	int output_plane_size;
	int input_plane_size;
	__declspec(align(16)) float weight[kMaxInput][kMaxOutput][sizeof(__m128) * 3];
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

/* �v���g�^�C�v�錾 */
void SetFilter(FILTER_PROC_INFO*, const int, const int);

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
		if(!fp->exfunc->is_saving(fpip->editp)) SetWindowText(fp->hwnd, _T("waifu2x-cpu"));
		return TRUE;
	}
	if(!fp->exfunc->is_saving(fpip->editp)){
		SetWindowText(fp->hwnd, _T("waifu2x-cpu(������...)"));
		start = std::chrono::system_clock::now();
	}

	// �ݒ�ɉ����ă��f���f�[�^��I�����A�������s��
	if(fp->track[kTrackNoise] > 0){
		// �m�C�Y��������ꍇ
		SetFilter(fpip, fp->track[kTrackNoise] - 1, fp->track[kTrackThread]);
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
		SetFilter(fpip, kModelScale2x, fp->track[kTrackThread]);
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

void SetFilter(FILTER_PROC_INFO *fpip, const int mode_, const int thread_){
	int steps_size = g_models[mode_].steps.size();
	// Y������[0,1]�ɐ��K������
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
	// �ӂ̕������g������
	for(auto y = 0; y < steps_size; ++y){	//����
		for(auto x = 0; x < steps_size; ++x){
			input_picture_y[0][x][y] = input_picture_y[0][steps_size][steps_size];
		}
	}
	for(auto y = 0; y < steps_size; ++y){
		for(auto x = steps_size; x < fpip->w + steps_size; ++x){	//��
			input_picture_y[0][x][y] = input_picture_y[0][x][steps_size];
		}
	}
	for(auto y = 0; y < steps_size; ++y){	//�E��
		for(auto x = fpip->w + steps_size; x < fpip->w + steps_size * 2; ++x){
			input_picture_y[0][x][y] = input_picture_y[0][fpip->w + steps_size - 1][steps_size];
		}
	}
	for(auto y = steps_size; y < fpip->h + steps_size; ++y){	//�E
		for(auto x = fpip->w + steps_size; x < fpip->w + steps_size * 2; ++x){
			input_picture_y[0][x][y] = input_picture_y[0][fpip->w + steps_size - 1][y];
		}
	}
	for(auto y = fpip->h + steps_size; y < fpip->h + steps_size * 2; ++y){	//�E��
		for(auto x = fpip->w + steps_size; x < fpip->w + steps_size * 2; ++x){
			input_picture_y[0][x][y] = input_picture_y[0][fpip->w + steps_size - 1][fpip->h + steps_size - 1];
		}
	}
	for(auto y = fpip->h + steps_size; y < fpip->h + steps_size * 2; ++y){	//��
		for(auto x = steps_size; x < fpip->w + steps_size; ++x){
			input_picture_y[0][x][y] = input_picture_y[0][x][fpip->h + steps_size - 1];
		}
	}
	for(auto y = fpip->h + steps_size; y < fpip->h + steps_size * 2; ++y) {	//����
		for(auto x = 0; x < steps_size; ++x){
			input_picture_y[0][x][y] = input_picture_y[0][steps_size][fpip->h + steps_size - 1];
		}
	}
	for(auto y = steps_size; y < fpip->h + steps_size; ++y){//��
		for(auto x = 0; x < steps_size; ++x){
			input_picture_y[0][x][y] = input_picture_y[0][steps_size][y];
		}
	}
	// ���C�����[�v
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
		// ��ݍ��݉��Z
		#pragma omp parallel for num_threads(thread_)
		for(auto o = 0; o < output_plane_size; ++o){
			// 3x3�̃t�B���^����
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
						output_picture_y[o][x][y] += sum;
					}
				}
			}
			// �o�C�A�X���|����
			for(auto y = 0; y < y_size; ++y){
				for(auto x = 0; x < x_size; ++x){
					output_picture_y[o][x][y] += step_data->bias[o];
				}
			}
		}
		// ���X�e�b�v�̂��߂ɒ�������
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

	// ���ʂ�߂�
	for(auto y = 0; y < fpip->h; y++) {
		auto ycp = fpip->ycp_edit + y * fpip->max_w;
		for(auto x = 0; x < fpip->w; x++) {
			ycp->y = static_cast<short>(round(input_picture_y[0][x][y] * 4096));
			ycp++;
		}
	}
}
