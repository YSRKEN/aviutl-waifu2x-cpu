#aviutl-waifu2x-cpu
waifu2x�̏������s�����Ƃ��\�ȁAAviUtl�̃t�B���^�v���O�C���ł��B

##�C���X�g�[��
plugins�ȉ���DL����AviUtl�̃t�H���_�ɓ˂�����ł��������B
���̃v���O�C����aviutl.exe�Ɠ��t�H���_���Ɠ��삵�܂��񂵁A
models�t�H���_�͓��Yauf�t�@�C���Ɠ����t�H���_�ɂ���K�v������܂��B

##�g�p���@
�ݒ��ʂ�noise�g���b�N�o�[��scale�g���b�N�o�[�����̃V���v���Ȃ��̂ł��B
���ꂼ��f�m�C�Y���x��0�`2�A�g�僌�x��0�`1(0���Ɗg�債�Ȃ�)�ł��B
����0���Ɖ������܂��񂪁A����ȊO�Ȃ�f�m�C�Y���g�傩�������s���܂��B
�Ȃ��A�ݒ��ʂ̃^�C�g���o�[�ɉ��Z���Ԃ��~���b�P�ʂŕ\������܂��B

##�R���p�C��
Microsoft Visual Studio 2013�ŃR���p�C�����܂����B
AviUtl�Ȃ̂�32bit�o�C�i�����쐬���Ă��������B

##���f���f�[�^�ɂ���
tool�t�H���_����jsoncnv.exe������܂����A����Ō��X��JSON�f�[�^��ϊ����Ă��܂��B
JSON�f�[�^�̉��߂ɂ�[picojson.h](https://github.com/kazuho/picojson)���g�p���܂����B
�������ďo�������f���f�[�^(�o�C�i��)�̃t�H�[�}�b�g�͎��̒ʂ�ł��B
 * �X�e�b�v��(int)
 * ���͕��ʐ�(int)�Əo�͕��ʐ�(int)�̃Z�b�g���X�e�b�v����J��Ԃ�
 * �d�݃f�[�^(double)���o�͕��ʐ��~���͕��ʐ��~3�~3��J��Ԃ����Ƃ��X�e�b�v����J��Ԃ�
 * �o�C�A�X(double)���o�͕��ʐ���J��Ԃ����Ƃ��X�e�b�v����J��Ԃ�
�ϊ�����ۂ́A�R�}���h���C�����������̂悤�ɐݒ肵�܂��B
�ujsoncnv {�ϊ�������json�t�@�C��} {�o�͌��dat�t�@�C��}�v
�Ȃ��Ajsoncnv.exe�̃\�[�X�R�[�h��jsoncnv.cpp�ł��B

##���̑�
sample�t�H���_�ɂ̓T���v���摜��u���Ă���܂��B
Core i5-3210M��100x100�̉摜��noise�����10�b�Escale��40�b�Enoise_scale��50�b���x
�|����܂����A�����I�ɂ�SIMD�E���񏈗���������č������\��ł��B
