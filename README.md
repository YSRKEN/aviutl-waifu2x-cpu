#aviutl-waifu2x-cpu
waifu2x�̏������s�����Ƃ��\�ȁAAviUtl�̃t�B���^�v���O�C���ł��B

##�C���X�g�[��
plugins�ȉ���DL����AviUtl�̃t�H���_�ɓ˂�����ł��������B
�܂��Avcomp120.dll��aviutl.exe�Ɠ����t�H���_�ɒu���Ă��������B
���̃v���O�C����aviutl.exe�Ɠ��t�H���_���Ɠ��삵�܂��񂵁A
models�t�H���_�͓��Yauf�t�@�C���Ɠ����t�H���_�ɂ���K�v������܂��B
���Ȃ݂�SSE�EAVX�EFMA�p�Ƀt�@�C����������Ă��܂��̂ŁA
�����ɓK�����z��I�����Ă��������ȁB

##�g�p���@
�ݒ��ʂ͎��̒ʂ�B�f�t�H���g�ł�1�E0�E0�E32�E�`�F�b�N�Ȃ��ɃZ�b�g����Ă��܂��B
 * thread�g���b�N�o�[�c�c�X���b�h��
 * noise�g���b�N�o�[�c�c�f�m�C�Y���x��(0�Ńf�m�C�Y���Ȃ�)
 * scale�g���b�N�o�[�c�c�g�傷�邩�ۂ�(0���Ɗg�債�Ȃ�)
 * block�g���b�N�o�[�c�c�u���b�N�ɕ�������ۂ�1�T�C�Y�B�摜�̏c���̌��񐔂ɂ��Ă�����OK���ƁB
 * use blocking�`�F�b�N�{�b�N�X�c�c�����͕s�v�ł��傤�B
�Ȃ��A�ݒ��ʂ̃^�C�g���o�[�ɉ��Z���Ԃ��~���b�P�ʂŕ\������܂��B
noise��scale��0�ɂ���΁A�t�B���^����������������ŏ��̕\���ɖ߂�܂��B

##�R���p�C��
Microsoft Visual Studio 2013�ŃR���p�C�����܂����B
AviUtl�Ȃ̂�32bit�o�C�i�����쐬���Ă��������B

##���f���f�[�^�ɂ���
tool�t�H���_����jsoncnv_.exe������܂����A����Ō��X��JSON�f�[�^��ϊ����Ă��܂��B
JSON�f�[�^�̉��߂ɂ�[picojson.h](https://github.com/kazuho/picojson)���g�p���܂����B
�������ďo�������f���f�[�^(�o�C�i��)�̃t�H�[�}�b�g�͎��̒ʂ�ł��B
 * �X�e�b�v��(int)
 * ���͕��ʐ�(int)�Əo�͕��ʐ�(int)�̃Z�b�g���X�e�b�v����J��Ԃ�
 * �d�݃f�[�^(float)���o�͕��ʐ��~���͕��ʐ��~3�~3��J��Ԃ����Ƃ��X�e�b�v����J��Ԃ�
 * �o�C�A�X(float)���o�͕��ʐ���J��Ԃ����Ƃ��X�e�b�v����J��Ԃ�
�ϊ�����ۂ́A�R�}���h���C�����������̂悤�ɐݒ肵�܂��B
�ujsoncnv_ {�ϊ�������json�t�@�C��} {�o�͌��dat�t�@�C��}�v
�Ȃ��Ajsoncnv_.exe�̃\�[�X�R�[�h��jsoncnv_.cpp�ł��B

##���̑�
sample�t�H���_�ɂ̓T���v���摜��u���Ă���܂��B

##�X�V����
Ver.1.2
�������z�u�ɍ��킹�ēY���̏��Ԃ����ւ����B
������������x�������Ȃ�悤�ɑ啝�ȉ��ǂ��������B
�u���b�L���O�������������A���ʂł��������s���ɂȂ�Ȃ��悤�ɂ����B
�֊s����̏����������ɂ��A����Y��ɕϊ��ł���悤�ɂ����B
SSE�ł����łȂ��AAVX�EFMA�ł��p�ӂ����B

Ver.1.1
weight��bias�̐��x��float�ɕύX�B����ɔ����Atool��jsoncnv����������������jsoncnv_�Ƃ����B�܂��A����ɂ�胂�f���f�[�^�̃t�@�C���T�C�Y���팸����Ă��܂��B
SIMD�Ƃ���SSE�A����v�Z�Ƃ���OpenMP�ɑΉ��B

Ver.1.0
���ŁB