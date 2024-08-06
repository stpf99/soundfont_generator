for source compillation https://github.com/stpf99/sf2cute    | libsndfile

g++ -std=c++17 -o soundfont_generator soundfont_generator.cpp -lsf2cute -lsndfile

prepare wav files:

ffmpeg -i input.wav -ac 1 -acodec pcm_s16le output.wav



./soundfont_generator input_wav_files/

