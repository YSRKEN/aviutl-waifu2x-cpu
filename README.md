#aviutl-waifu2x-cpu
waifu2xの処理を行うことが可能な、AviUtlのフィルタプラグインです。

##インストール
plugins以下をDLしてAviUtlのフォルダに突っ込んでください。
また、vcomp120.dllをaviutl.exeと同じフォルダに置いてください。
このプラグインはaviutl.exeと同フォルダだと動作しませんし、
modelsフォルダは当該aufファイルと同じフォルダにある必要があります。
ちなみにSSE・AVX・FMA用にファイルが分かれていますので、
自環境に適した奴を選択してくださいな。

##使用方法
設定画面は次の通り。デフォルトでは1・0・0・32・チェックなしにセットされています。
 * threadトラックバー……スレッド数
 * noiseトラックバー……デノイズレベル(0でデノイズしない)
 * scaleトラックバー……拡大するか否か(0だと拡大しない)
 * blockトラックバー……ブロックに分割する際の1サイズ。画像の縦横の公約数にしておけばOKかと。
 * use blockingチェックボックス……説明は不要でしょう。
なお、設定画面のタイトルバーに演算時間がミリ秒単位で表示されます。
noiseとscaleを0にすれば、フィルタ処理が無効化され最初の表示に戻ります。

##コンパイル
Microsoft Visual Studio 2013でコンパイルしました。
AviUtlなので32bitバイナリを作成してください。

##モデルデータについて
toolフォルダ内にjsoncnv_.exeがありますが、これで元々のJSONデータを変換しています。
JSONデータの解釈には[picojson.h](https://github.com/kazuho/picojson)を使用しました。
そうして出来たモデルデータ(バイナリ)のフォーマットは次の通りです。
 * ステップ数(int)
 * 入力平面数(int)と出力平面数(int)のセットをステップ数回繰り返す
 * 重みデータ(float)を出力平面数×入力平面数×3×3回繰り返すことをステップ数回繰り返す
 * バイアス(float)を出力平面数回繰り返すことをステップ数回繰り返す
変換する際は、コマンドライン引数を次のように設定します。
「jsoncnv_ {変換したいjsonファイル} {出力後のdatファイル}」
なお、jsoncnv_.exeのソースコードはjsoncnv_.cppです。

##その他
sampleフォルダにはサンプル画像を置いてあります。

##更新履歴
Ver.1.2
メモリ配置に合わせて添字の順番を入れ替えた。
処理をより並列度が高くなるように大幅な改良を加えた。
ブロッキング処理を実装し、大画面でもメモリ不足にならないようにした。
輪郭周りの処理を完璧にし、より綺麗に変換できるようにした。
SSE版だけでなく、AVX・FMA版も用意した。

Ver.1.1
weightとbiasの精度をfloatに変更。それに伴い、toolのjsoncnvも多少書き換えてjsoncnv_とした。また、これによりモデルデータのファイルサイズが削減されています。
SIMDとしてSSE、並列計算としてOpenMPに対応。

Ver.1.0
初版。

##参考にした資料
・「SSEとAVXで高次元ベクトルの内積計算を高速化してみた」
[http://daily.belltail.jp/?p=1520](http://daily.belltail.jp/?p=1520)
・「組み込み関数（intrinsic）によるSIMD入門」
[http://www.slideshare.net/FukushimaNorishige/simd-10548373](http://www.slideshare.net/FukushimaNorishige/simd-10548373)
・「x86 SIMD Technique:: 水平加算での合計計算」
[http://www.kaede-software.com/2014/04/post_641.html](http://www.kaede-software.com/2014/04/post_641.html)
・「インテル® C++ コンパイラー XE 13.1 ユーザー・リファレンス・ガイド 」
[http://nf.nci.org.au/facilities/software/intel-ct/13.5.192/Documentation/ja_JP/compiler_c/main_cls/index.htm](http://nf.nci.org.au/facilities/software/intel-ct/13.5.192/Documentation/ja_JP/compiler_c/main_cls/index.htm)
