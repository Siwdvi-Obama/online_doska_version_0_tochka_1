#include <iostream>
#include <cmath>
#include <math.h>
#include <string>
#include <vector>
#include <limits>
#include "reasings.h"

using namespace std;

const int GAME_WIDTH = 640, GAME_HEIGHT = 360, PIXEL_SIZE = 2;

int min(int a, const unsigned int b)
{
    if (a < b){
        return a;
    }
    return b;
}
float min(float a, float b){
    if (a < b){
        return a;
    }
    return b;
}
float max(float a, float b){
    if (a > b){
        return a;
    }
    return b;
}

float angle(Vector2 a, Vector2 b={0, 1})
{
	if (b.y >= a.y){
		return acos((b.x - a.x) / sqrt((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y)));
	}
	else{
		return 2 * PI - acos((b.x - a.x) / sqrt((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y)));
	}
}
float angle(Vector2 a, int bx, int by)
{
	if (by >= a.y){
		return acos((bx - a.x) / sqrt((bx - a.x) * (bx - a.x) + (by - a.y) * (by - a.y)));
	}
	else{
		return 2 * PI - acos((bx - a.x) / sqrt((bx - a.x) * (bx - a.x) + (by - a.y) * (by - a.y)));
	}
}
float rad_to_deg(float r)
{
	return r * 180 / PI;
}
float deg_to_rad(float d)
{
	return d * PI / 180;
}
float dist(Vector2 a, Vector2 b)
{
	return sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
}
float norm_angle(float a){
    a = fmod(a, 360.0f);
    if (a < 0) a += 360.0f;
    return a;
}
bool angle_in_arc(float angle, float start, float end){
    if (start == end) return true;
    angle = norm_angle(angle);
    start = norm_angle(start);
    end = norm_angle(end);
    if (start < end){
        return angle > start && angle < end;
    }
    else{
        return angle > start || angle < end;
    }
}
Vector2 GetMousePosition_()
{
	return Vector2{GetMousePosition().x * GAME_WIDTH / GetScreenWidth(), GetMousePosition().y * GAME_HEIGHT / GetScreenHeight()};
}
Vector2 GetMousePositionCam_(Camera2D cam)
{
	return Vector2{(GetMousePosition().x) * GAME_WIDTH / GetScreenWidth() / cam.zoom + cam.target.x - cam.offset.x / cam.zoom, (GetMousePosition().y) * GAME_HEIGHT / GetScreenHeight() / cam.zoom + cam.target.y - cam.offset.y / cam.zoom};
}
Vector2 GetMousePositionCam(Camera2D cam)
{
	return Vector2{GetMousePosition().x / cam.zoom + cam.target.x - cam.offset.x / cam.zoom, GetMousePosition().y / cam.zoom + cam.target.y - cam.offset.y / cam.zoom};
}

struct Speed
{
	float ang;
	float vel;
	Speed();
	Speed(Vector2 v);
	Speed(float a, float v);
	Vector2 to_Vector2();
	float get_x(){return to_Vector2().x;}
	float get_y(){return to_Vector2().y;}
	void set_x(float x){Speed s = Speed(Vector2{x, get_y()}); ang = s.ang; vel = s.vel;}
	void set_y(float y){Speed s = Speed(Vector2{get_x(), y}); ang = s.ang; vel = s.vel;}
    float vel_along_ang(float angle);
    void fix();
};
Speed::Speed()
{
	ang = 0;
	vel = 0;
}
Speed::Speed(Vector2 v)
{
	ang = rad_to_deg(angle(Vector2{0, 0}, v));
	vel = sqrt(v.x * v.x + v.y * v.y);
}
Speed::Speed(float a, float v)
{
	ang = a;
	vel = v;
}
Vector2 Speed::to_Vector2()
{
	float x_ = vel * cos(deg_to_rad(ang));
	float y_ = vel * sin(deg_to_rad(ang));
	return Vector2{x_, y_};
}
float Speed::vel_along_ang(float angle)
{
    float diff = min(abs(angle - ang), 360 - abs(angle - ang));
    return cos(deg_to_rad(diff)) * vel;
}
void Speed::fix()
{
    if (vel < 0){
        vel = -vel;
        ang += 180;
    }
    if (ang < 0){
        ang += 360;
    }
    if (ang > 360){
        ang -= 360;
    }
}
Speed &operator+=(Speed &s, Vector2 v)
{
	s = Speed(Vector2{v.x + s.get_x(), v.y + s.get_y()});
	return s;
}
Speed &operator+=(Speed &s1, Speed s2)
{
    Vector2 v = s2.to_Vector2();
	s1 = Speed(Vector2{v.x + s1.to_Vector2().x, v.y + s1.to_Vector2().y});
	return s1;
}
Speed operator+(Speed s1, Speed s2)
{
    Vector2 v = s2.to_Vector2();
	return Speed(Vector2{v.x + s1.to_Vector2().x, v.y + s1.to_Vector2().y});
}
Vector2 operator+(Vector2 v1, Vector2 v2)
{
	return Vector2{v1.x + v2.x, v1.y + v2.y};
}
Vector2 operator-(Vector2 v1, Vector2 v2)
{
	return Vector2{v1.x - v2.x, v1.y - v2.y};
}
Speed operator*(Speed s, float f)
{
    return Speed(s.ang, s.vel * f);
}
Speed operator/(Speed s, float f)
{
    return Speed(s.ang, s.vel / f);
}
Vector2 &operator+=(Vector2 &v, Speed s)
{
	v.x += s.to_Vector2().x;
	v.y += s.to_Vector2().y;
	return v;
}
Vector2 operator+(Vector2 a, Speed s)
{
    Vector2 b = s.to_Vector2();
    return Vector2{a.x + b.x, a.y + b.y};
}

struct AdaptiveMusic
{
	Music music;
	float* volume;
	int mode = 0;
};
/*AdaptiveMusic LoadAdaptiveMusic(const char *fileName)
{
    Music music = { 0 };
    bool musicLoaded = false;

    if (false) { }
#if defined(SUPPORT_FILEFORMAT_WAV)
    else if (IsFileExtension(fileName, ".wav"))
    {
        drwav *ctxWav = RL_CALLOC(1, sizeof(drwav));
        bool success = drwav_init_file(ctxWav, fileName, NULL);

        if (success)
        {
            music.ctxType = MUSIC_AUDIO_WAV;
            music.ctxData = ctxWav;
            int sampleSize = ctxWav->bitsPerSample;
            if (ctxWav->bitsPerSample == 24) sampleSize = 16;   // Forcing conversion to s16 on UpdateMusicStream()

            music.stream = LoadAudioStream(ctxWav->sampleRate, sampleSize, ctxWav->channels);
            music.frameCount = (unsigned int)ctxWav->totalPCMFrameCount;
            music.looping = true;   // Looping enabled by default
            musicLoaded = true;
        }
        else
        {
            RL_FREE(ctxWav);
        }
    }
#endif
#if defined(SUPPORT_FILEFORMAT_OGG)
    else if (IsFileExtension(fileName, ".ogg"))
    {
        // Open ogg audio stream
        stb_vorbis *ctxOgg = stb_vorbis_open_filename(fileName, NULL, NULL);

        if (ctxOgg != NULL)
        {
            music.ctxType = MUSIC_AUDIO_OGG;
            music.ctxData = ctxOgg;
            stb_vorbis_info info = stb_vorbis_get_info((stb_vorbis *)music.ctxData);  // Get Ogg file info

            // OGG bit rate defaults to 16 bit, it's enough for compressed format
            music.stream = LoadAudioStream(info.sample_rate, 16, info.channels);

            // WARNING: It seems this function returns length in frames, not samples, so we multiply by channels
            music.frameCount = (unsigned int)stb_vorbis_stream_length_in_samples((stb_vorbis *)music.ctxData);
            music.looping = true;   // Looping enabled by default
            musicLoaded = true;
        }
        else
        {
            stb_vorbis_close(ctxOgg);
        }
    }
#endif
#if defined(SUPPORT_FILEFORMAT_MP3)
    else if (IsFileExtension(fileName, ".mp3"))
    {
        drmp3 *ctxMp3 = RL_CALLOC(1, sizeof(drmp3));
        int result = drmp3_init_file(ctxMp3, fileName, NULL);

        if (result > 0)
        {
            music.ctxType = MUSIC_AUDIO_MP3;
            music.ctxData = ctxMp3;
            music.stream = LoadAudioStream(ctxMp3->sampleRate, 32, ctxMp3->channels);
            music.frameCount = (unsigned int)drmp3_get_pcm_frame_count(ctxMp3);
            music.looping = true;   // Looping enabled by default
            musicLoaded = true;
        }
        else
        {
            RL_FREE(ctxMp3);
        }
    }
#endif
#if defined(SUPPORT_FILEFORMAT_QOA)
    else if (IsFileExtension(fileName, ".qoa"))
    {
        qoaplay_desc *ctxQoa = qoaplay_open(fileName);

        if (ctxQoa != NULL)
        {
            music.ctxType = MUSIC_AUDIO_QOA;
            music.ctxData = ctxQoa;
            // NOTE: We are loading samples are 32bit float normalized data, so,
            // we configure the output audio stream to also use float 32bit
            music.stream = LoadAudioStream(ctxQoa->info.samplerate, 32, ctxQoa->info.channels);
            music.frameCount = ctxQoa->info.samples;
            music.looping = true;   // Looping enabled by default
            musicLoaded = true;
        }
        else{} //No uninit required
    }
#endif
#if defined(SUPPORT_FILEFORMAT_FLAC)
    else if (IsFileExtension(fileName, ".flac"))
    {
        drflac *ctxFlac = drflac_open_file(fileName, NULL);

        if (ctxFlac != NULL)
        {
            music.ctxType = MUSIC_AUDIO_FLAC;
            music.ctxData = ctxFlac;
            int sampleSize = ctxFlac->bitsPerSample;
            if (ctxFlac->bitsPerSample == 24) sampleSize = 16;   // Forcing conversion to s16 on UpdateMusicStream()
            music.stream = LoadAudioStream(ctxFlac->sampleRate, sampleSize, ctxFlac->channels);
            music.frameCount = (unsigned int)ctxFlac->totalPCMFrameCount;
            music.looping = true;   // Looping enabled by default
            musicLoaded = true;
        }
        else
        {
            drflac_free(ctxFlac, NULL);
        }
    }
#endif
#if defined(SUPPORT_FILEFORMAT_XM)
    else if (IsFileExtension(fileName, ".xm"))
    {
        jar_xm_context_t *ctxXm = NULL;
        int result = jar_xm_create_context_from_file(&ctxXm, AUDIO.System.device.sampleRate, fileName);

        if (result == 0)    // XM AUDIO.System.context created successfully
        {
            music.ctxType = MUSIC_MODULE_XM;
            music.ctxData = ctxXm;
            jar_xm_set_max_loop_count(ctxXm, 0);    // Set infinite number of loops

            unsigned int bits = 32;
            if (AUDIO_DEVICE_FORMAT == ma_format_s16) bits = 16;
            else if (AUDIO_DEVICE_FORMAT == ma_format_u8) bits = 8;

            // NOTE: Only stereo is supported for XM
            music.stream = LoadAudioStream(AUDIO.System.device.sampleRate, bits, AUDIO_DEVICE_CHANNELS);
            music.frameCount = (unsigned int)jar_xm_get_remaining_samples(ctxXm);    // NOTE: Always 2 channels (stereo)
            music.looping = true;   // Looping enabled by default
            jar_xm_reset(ctxXm);    // Make sure we start at the beginning of the song
            musicLoaded = true;
        }
        else
        {
            jar_xm_free_context(ctxXm);
        }
    }
#endif
#if defined(SUPPORT_FILEFORMAT_MOD)
    else if (IsFileExtension(fileName, ".mod"))
    {
        jar_mod_context_t *ctxMod = RL_CALLOC(1, sizeof(jar_mod_context_t));
        jar_mod_init(ctxMod);
        int result = jar_mod_load_file(ctxMod, fileName);

        if (result > 0)
        {
            music.ctxType = MUSIC_MODULE_MOD;
            music.ctxData = ctxMod;
            // NOTE: Only stereo is supported for MOD
            music.stream = LoadAudioStream(AUDIO.System.device.sampleRate, 16, AUDIO_DEVICE_CHANNELS);
            music.frameCount = (unsigned int)jar_mod_max_samples(ctxMod);    // NOTE: Always 2 channels (stereo)
            music.looping = true;   // Looping enabled by default
            musicLoaded = true;
        }
        else
        {
            jar_mod_unload(ctxMod);
            RL_FREE(ctxMod);
        }
    }
#endif
    else TRACELOG(LOG_WARNING, "STREAM: [%s] File format not supported", fileName);

    if (!musicLoaded)
    {
        TRACELOG(LOG_WARNING, "FILEIO: [%s] Music file could not be opened", fileName);
    }
    else
    {
        // Show some music stream info
        TRACELOG(LOG_INFO, "FILEIO: [%s] Music file loaded successfully", fileName);
        TRACELOG(LOG_INFO, "    > Sample rate:   %i Hz", music.stream.sampleRate);
        TRACELOG(LOG_INFO, "    > Sample size:   %i bits", music.stream.sampleSize);
        TRACELOG(LOG_INFO, "    > Channels:      %i (%s)", music.stream.channels, (music.stream.channels == 1)? "Mono" : (music.stream.channels == 2)? "Stereo" : "Multi");
        TRACELOG(LOG_INFO, "    > Total frames:  %i", music.frameCount);
    }

    return music;
}*/