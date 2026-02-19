#include "audio_loader.h"

#include <miniaudio.h>
#include <spdlog/spdlog.h>

#include <string>
#include <vector>

namespace engine::resource {

namespace {
constexpr std::size_t DECODE_CHUNK_FRAMES = 4096;
} // namespace

AudioLoader::result_type AudioLoader::operator()(std::string_view file_path) const {
    if (file_path.empty()) {
        spdlog::error("AudioLoader: decodeAudio 失败，提供的文件路径为空。");
        return {};
    }

    // 仅使用 miniaudio 声明；实现由 audio_player.cpp 中的 MINIAUDIO_IMPLEMENTATION 提供。
    const std::string path_str(file_path);
    ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);
    ma_decoder decoder{};
    const ma_result init_result = ma_decoder_init_file(path_str.c_str(), &config, &decoder);
    if (init_result != MA_SUCCESS) {
        spdlog::error("AudioLoader: 无法解码音频文件 '{}', 错误码 {}", file_path, static_cast<int>(init_result));
        return {};
    }

    const std::uint32_t channels = decoder.outputChannels;
    const std::uint32_t sample_rate = decoder.outputSampleRate;
    if (channels == 0 || sample_rate == 0) {
        spdlog::error("AudioLoader: 解码音频文件 '{}' 失败，通道数或采样率无效。", file_path);
        ma_decoder_uninit(&decoder);
        return {};
    }

    std::vector<float> samples;
    samples.reserve(DECODE_CHUNK_FRAMES * channels);

    std::vector<float> read_buffer(DECODE_CHUNK_FRAMES * channels);
    std::uint64_t total_frames = 0;

    while (true) {
        ma_uint64 frames_read = 0;
        const ma_result read_result = ma_decoder_read_pcm_frames(
            &decoder,
            read_buffer.data(),
            DECODE_CHUNK_FRAMES,
            &frames_read
        );

        if ((read_result != MA_SUCCESS && read_result != MA_AT_END) || frames_read == 0) {
            if (read_result != MA_SUCCESS && read_result != MA_AT_END) {
                spdlog::error("AudioLoader: 读取音频文件 '{}' 时出错，错误码 {}", file_path, static_cast<int>(read_result));
            }
            break;
        }

        const std::size_t sample_count = static_cast<std::size_t>(frames_read) * channels;
        samples.insert(samples.end(), read_buffer.data(), read_buffer.data() + sample_count);
        total_frames += frames_read;

        if (read_result == MA_AT_END) {
            break;
        }
    }

    ma_decoder_uninit(&decoder);

    if (total_frames == 0 || samples.empty()) {
        spdlog::warn("AudioLoader: 解码文件 '{}' 未获得有效音频数据。", file_path);
        return {};
    }

    auto buffer = std::make_shared<AudioBuffer>();
    buffer->samples = std::move(samples);
    buffer->channels = channels;
    buffer->sample_rate = sample_rate;
    buffer->frame_count = total_frames;

    spdlog::debug(
        "AudioLoader: 成功解码音频 '{}'，通道数={}, 采样率={}, 帧数={}",
        file_path,
        channels,
        sample_rate,
        total_frames
    );

    return buffer;
}

} // namespace engine::resource
