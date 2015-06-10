/* waifu2x-cpu Ver.1.0 by YSR */

/* �v���v���Z�b�T */
#include <chrono>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <tchar.h>
#include <vector>
#include <windows.h>
#include "filter.h"

const int kTracks = 2;							//�g���b�N�o�[�̐�
TCHAR	*track_name[] =		{"noise", "scale"};	//�g���b�N�o�[�̖��O
int		track_default[] =	{0,       0};		//�g���b�N�o�[�̏����l
int		track_s[] =			{0,       0};		//�g���b�N�o�[�̉����l
int		track_e[] =			{2,       1};		//�g���b�N�o�[�̏���l

/* �e���v�� */
FILTER_DLL filter = {
	FILTER_FLAG_EX_INFORMATION,
	0,0,						//�ݒ�E�C���h�E�̃T�C�Y (FILTER_FLAG_WINDOW_SIZE�������Ă��鎞�ɗL��)
	"waifu2x-cpu",				//�t�B���^�̖��O
	kTracks,					//�g���b�N�o�[�̐� (0�Ȃ疼�O�����l����NULL�ł悢)
	track_name,					//�g���b�N�o�[�̖��O�S�ւ̃|�C���^
	track_default,				//�g���b�N�o�[�̏����l�S�ւ̃|�C���^
	track_s,track_e,			//�g���b�N�o�[�̐��l�̉������ (NULL�Ȃ�S��0�`256)
	NULL,						//�`�F�b�N�{�b�N�X�̐� (0�Ȃ疼�O�����l����NULL�ł悢)
	NULL,						//�`�F�b�N�{�b�N�X�̖��O�S�ւ̃|�C���^
	NULL,						//�`�F�b�N�{�b�N�X�̏����l�S�ւ̃|�C���^
	func_proc,					//�t�B���^�����֐��ւ̃|�C���^ (NULL�Ȃ�Ă΂�܂���)
	func_init,					//�J�n���ɌĂ΂��֐��ւ̃|�C���^ (NULL�Ȃ�Ă΂�܂���)
	NULL,						//�I�����ɌĂ΂��֐��ւ̃|�C���^ (NULL�Ȃ�Ă΂�܂���)
	NULL,						//�ݒ肪�ύX���ꂽ�Ƃ��ɌĂ΂��֐��ւ̃|�C���^ (NULL�Ȃ�Ă΂�܂���)
	NULL,						//�ݒ�E�B���h�E�ɃE�B���h�E���b�Z�[�W���������ɌĂ΂��֐��ւ̃|�C���^ (NULL�Ȃ�Ă΂�܂���)
	NULL,NULL,					//�V�X�e���Ŏg���܂��̂Ŏg�p���Ȃ��ł�������
	NULL,						//�g���f�[�^�̈�ւ̃|�C���^ (FILTER_FLAG_EX_DATA�������Ă��鎞�ɗL��)
	NULL,						//�g���f�[�^�T�C�Y (FILTER_FLAG_EX_DATA�������Ă��鎞�ɗL��)
	"waifu2x-cpu version 1.0 by YSR",
								//�t�B���^���ւ̃|�C���^ (FILTER_FLAG_EX_INFORMATION�������Ă��鎞�ɗL��)
	NULL,						//�Z�[�u���J�n����钼�O�ɌĂ΂��֐��ւ̃|�C���^ (NULL�Ȃ�Ă΂�܂���)
	NULL,						//�Z�[�u���I���������O�ɌĂ΂��֐��ւ̃|�C���^ (NULL�Ȃ�Ă΂�܂���)
};

EXTERN_C FILTER_DLL __declspec(dllexport) * __stdcall GetFilterTable(void){return &filter;}

/* using�錾 */
using std::stod;
using std::stoi;
using std::string;
using std::vector;

/* �萔�錾 */
enum kModelKind { kModelDenoise1, kModelDenoise2, kModelScale2x, kModels };
const int kMaxInput   = 128;
const int kMaxOutput  = 128;
const int kWidthSize  = 3;
const int kHeightSize = 3;

/* �N���X��` */
// 1�X�e�b�v�ɂ�����f�[�^
struct Step{
	int output_plane_size;
	int input_plane_size;
	double weight[kMaxInput][kMaxOutput][kWidthSize][kHeightSize];
	vector<double> bias;
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
					for(auto w = 0; w < kWidthSize; ++w){
						for(auto h = 0; h < kHeightSize; ++h){
							fin.read(reinterpret_cast<char*>(&steps[s].weight[o][i][w][h]), sizeof(double));
						}
					}
				}
			}
		}
		// bias
		for(auto s = 0; s < steps_size; ++s){
			steps[s].bias.resize(steps[s].output_plane_size);
			for(auto o = 0; o < steps[s].output_plane_size; ++o){
				fin.read(reinterpret_cast<char*>(&steps[s].bias[o]), sizeof(double));
			}
		}
	}
};

/* �v���g�^�C�v�錾 */
void SetFilter(FILTER_PROC_INFO*, const int);

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
		g_models[kModelDenoise1].Init(".\\plugins\\models\\noise1_model2.dat");
		g_models[kModelDenoise2].Init(".\\plugins\\models\\noise2_model2.dat");
		g_models[kModelScale2x].Init( ".\\plugins\\models\\scale2.0x_model2.dat");
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
	if((fp->track[0] == 0) && (fp->track[1] == 0)){
		if(!fp->exfunc->is_saving(fpip->editp)) SetWindowText(fp->hwnd, _T("waifu2x-cpu"));
		return TRUE;
	}
	if(!fp->exfunc->is_saving(fpip->editp)){
		SetWindowText(fp->hwnd, _T("waifu2x-cpu(������...)"));
		start = std::chrono::system_clock::now();
	}

	// �ݒ�ɉ����ă��f���f�[�^��I�����A�������s��
	if(fp->track[0] > 0){
		// �m�C�Y��������ꍇ
		SetFilter(fpip, fp->track[0] - 1);
	}
	if(fp->track[1] > 0){
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
		SetFilter(fpip, kModelScale2x);
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

void SetFilter(FILTER_PROC_INFO *fpip, const int mode_){
	int steps_size = g_models[mode_].steps.size();
	// Y������[0,1]�ɐ��K������
	vector< vector < vector<double> > >input_picture_y(kMaxInput, vector < vector<double> >(fpip->w + steps_size * 2, vector<double>(fpip->h + steps_size * 2)));
	for(auto y = 0; y < fpip->h; y++) {
		auto ycp = fpip->ycp_edit + y * fpip->max_w;
		for(auto x = 0; x < fpip->w; x++) {
			double normalized_y = 1.0 * ycp->y / 4096.0;
			if(normalized_y < 0.0) normalized_y = 0.0;
			if(normalized_y > 1.0) normalized_y = 1.0;
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
	for(auto y = fpip->h + steps_size; y < fpip->h + steps_size * 2; ++y){	//����
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
	vector< vector < vector<double> > >output_picture_y(kMaxOutput, vector < vector<double> >(fpip->w + steps_size * 2, vector<double>(fpip->h + steps_size * 2)));
	int padding = (steps_size - 1) * 2;
	for(auto s = 0; s < steps_size; ++s){
		for(auto o = 0; o < g_models[mode_].steps[s].output_plane_size; ++o){
			for(auto y = 0; y < fpip->h + padding; ++y){
				for(auto x = 0; x < fpip->w + padding; ++x){
					output_picture_y[o][x][y] = 0.0;
				}
			}
		}
		// ��ݍ��݉��Z
		for(auto o = 0; o < g_models[mode_].steps[s].output_plane_size; ++o){
			// 3x3�̃t�B���^����
			for(auto i = 0; i < g_models[mode_].steps[s].input_plane_size; ++i){
				for(auto y = 0; y < fpip->h + padding; ++y){
					for(auto x = 0; x < fpip->w + padding; ++x){
						for(auto h = 0; h < kHeightSize; ++h){
							for(auto w = 0; w < kWidthSize; ++w){
								output_picture_y[o][x][y] += input_picture_y[i][x + w][y + h] * g_models[mode_].steps[s].weight[o][i][w][h];
							}
						}
					}
				}
			}
			// �o�C�A�X���|����
			for(auto y = 0; y < fpip->h + padding; ++y){
				for(auto x = 0; x < fpip->w + padding; ++x){
					output_picture_y[o][x][y] += g_models[mode_].steps[s].bias[o];
				}
			}
		}
		// ���X�e�b�v�̂��߂ɒ�������
		for(auto o = 0; o < g_models[mode_].steps[s].output_plane_size; ++o){
			for(auto y = 0; y < fpip->h + padding; ++y){
				for(auto x = 0; x < fpip->w + padding; ++x){
					input_picture_y[o][x][y] = output_picture_y[o][x][y];
					if(input_picture_y[o][x][y] < 0.0) input_picture_y[o][x][y] *= 0.1;
				}
			}
		}
		padding -= 2;
	}

	// ���ʂ�߂�
	for(auto y = 0; y < fpip->h; y++) {
		auto ycp = fpip->ycp_edit + y * fpip->max_w;
		for(auto x = 0; x < fpip->w; x++) {
			ycp->y = round(input_picture_y[0][x][y] * 4096);
			ycp++;
		}
	}
}
