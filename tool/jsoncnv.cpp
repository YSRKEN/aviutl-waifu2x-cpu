/* ���f���f�[�^�����ȗ��t�H�[�}�b�g�ɕϊ����� */
/* 1�s�ځF�X�e�b�v��
 * 2�s�ځ`�X�e�b�v��+1�s�ځF�X�e�b�v���Ƃ�output_plane_size��input_plane_size
 * �X�e�b�v��+2�s�ځ`�F1�񖈂�weight�����と�E�と�������E���̏��ŏ�����Ă���
 * X�s�ځ`�F�X�e�b�v���Ƃ�bias��������Ă���
 */
#include <fstream>
#include <iostream>
#include <string>
#include "picojson.h"

using std::cout;
using picojson::array;
using std::vector;

int main(int argc, char *argv[]){
	// �t�@�C����ifstream�œǂݍ���
	if(argc < 3) return -1;
	std::ifstream fin(argv[1]);
	if(fin.fail()) return -1;
	// ����>>���Z�q�ŗ������߂�
	picojson::value v;
	fin >> v;
	std::ofstream fout(argv[2], std::ios_base::out | std::ios_base::binary);
	if(!v.is<array>()) return -1;	//������\
	auto a = v.get<array>();	//�z��Ƃ��ēǂݎ��
	int a_size = a.size();
	fout.write(reinterpret_cast<const char*>(&a_size), sizeof(int));
	for(auto i = 0; i < a.size(); ++i){
		auto o = a[i].get<picojson::object>();
		int input  = stoi(o["nInputPlane"].to_str() );
		fout.write(reinterpret_cast<const char*>(&input), sizeof(int));
		int output = stoi(o["nOutputPlane"].to_str() );
		fout.write(reinterpret_cast<const char*>(&output), sizeof(int));
	}
	for(auto i = 0; i < a.size(); ++i){
		auto o = a[i].get<picojson::object>();
		auto w = o["weight"].get<array>();
		for(auto j = 0; j < w.size(); ++j){
			auto w2 = w[j].get<array>();
			for(auto k = 0; k < w2.size(); ++k){
				auto w3 = w2[k].get<array>();
				for(auto kw = 0; kw < w3.size(); ++kw){
					auto f = w3[kw].get<array>();
					for(auto kh = 0; kh < f.size(); ++kh){
						double weight = stod(f[kh].to_str());
						fout.write(reinterpret_cast<const char*>(&weight), sizeof(double));
					}
				}
			}
		}
	}
	for(auto i = 0; i < a.size(); ++i){
		auto o = a[i].get<picojson::object>();
		auto b = o["bias"].get<array>();
		for(auto j = 0; j < b.size(); ++j){
			double bias = stod(b[j].to_str());
			fout.write(reinterpret_cast<const char*>(&bias), sizeof(double));
		}
	}
}
