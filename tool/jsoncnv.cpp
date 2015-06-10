/* モデルデータを自己流フォーマットに変換する */
/* 1行目：ステップ数
 * 2行目～ステップ数+1行目：ステップごとのoutput_plane_sizeとinput_plane_size
 * ステップ数+2行目～：1回毎のweightが左上→右上→左下→右下の順で書かれている
 * X行目～：ステップごとのbiasが書かれている
 */
#include <fstream>
#include <iostream>
#include <string>
#include "picojson.h"

using std::cout;
using picojson::array;
using std::vector;

int main(int argc, char *argv[]){
	// ファイルをifstreamで読み込み
	if(argc < 3) return -1;
	std::ifstream fin(argv[1]);
	if(fin.fail()) return -1;
	// 直接>>演算子で流し込める
	picojson::value v;
	fin >> v;
	std::ofstream fout(argv[2], std::ios_base::out | std::ios_base::binary);
	if(!v.is<array>()) return -1;	//判定も可能
	auto a = v.get<array>();	//配列として読み取り
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
