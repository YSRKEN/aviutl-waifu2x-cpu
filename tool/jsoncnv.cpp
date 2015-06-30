/* ���f���f�[�^�����ȗ��̃o�C�i���t�H�[�}�b�g�ɕϊ�����\�t�g
 * usage: jsoncnv2.exe <input.json> <output.dat>
 * ����(���̃p�^�[����X���Ԃ��Ƃ�{X}�ƕ\��):
 * <�o�̓t�@�C��> ::= <model>{S}
 * <model>  ::= <nInputPlane(I)> <nOutputPlane(O)> <weight> <bias>
 * <weight>   ::= <weight_1>{O}
 * <weight_1> ::= <weight_2>{I}
 * <weight_2> ::= <weight_3>{kH}
 * <weight_3> ::= <weight_4>{kW}
 * <weight_4> ::= <Real Number>
 * <bias> ::= <Real Number)>{O}
 * ���ϊ��ߒ��ŁA�X�e�b�v����7�AkW��kH��3���Ɖ��肵�Ă��܂�
 */

#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include "picojson.h"

using std::cout;
using std::stoi;
using std::stod;
using std::vector;
using picojson::array;
using picojson::object;

typedef uint32_t Int;
typedef float Real;

const auto kWidthSize  = 3;
const auto kHeightSize = 3;

int main(int argc, char *argv[]){
	// �t�@�C����ifstream�œǂݍ���
	if(argc < 3) return -1;
	std::ifstream fin(argv[1]);
	if(fin.fail()) return -1;
	picojson::value input_json;
	fin >> input_json;
	// �o�͗p�t�@�C��������
	std::ofstream fout(argv[2], std::ios_base::out | std::ios_base::binary);
	// �ǂݍ��݂A�f�[�^����ɏ�������ł���
	auto models = input_json.get<array>();
	Int step_size = models.size();	//�X�e�b�v��
	for(auto s = 0; s < step_size; ++s){
		auto model = models[s].get<picojson::object>();
		// nInputPlane
		Int input_plane_size  = stoi(model["nInputPlane"].to_str());
		fout.write(reinterpret_cast<const char*>(&input_plane_size), sizeof(Int));
		// nOutputPlane
		Int output_plane_size = stoi(model["nOutputPlane"].to_str());
		fout.write(reinterpret_cast<const char*>(&output_plane_size), sizeof(Int));
		// weight
		auto weight = model["weight"].get<array>();
		for(auto o = 0; o < output_plane_size; ++o){
			auto weight_1 = weight[o].get<array>();
			for(auto i = 0; i < input_plane_size; ++i){
				// �������ލہA�ǂ݂₷���悤�ɓ]�u����
				auto weight_2 = weight_1[i].get<array>();
				Real weight_array[kHeightSize][kWidthSize];
				for(auto w = 0; w < kWidthSize; ++w){
					auto weight_3 = weight_2[w].get<array>();
					for(auto h = 0; h < kHeightSize; ++h){
						weight_array[h][w] = stod(weight_3[h].to_str());
					}
				}
				for(auto h = 0; h < kHeightSize; ++h){
					for(auto w = 0; w < kWidthSize; ++w){
						fout.write(reinterpret_cast<const char*>(&weight_array[h][w]), sizeof(Real));
					}
				}
			}
		}
		// bias
		auto bias = model["bias"].get<array>();
		for(auto o = 0; o < output_plane_size; ++o){
			Real temp = stod(bias[o].to_str());
			fout.write(reinterpret_cast<const char*>(&temp), sizeof(Real));
		}
	}
}
