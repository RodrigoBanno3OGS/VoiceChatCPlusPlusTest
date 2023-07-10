/*--------------------------------------------------------------------------------*
  Copyright Nintendo.  All rights reserved.

  These coded instructions, statements, and computer programs contain proprietary
  information of Nintendo and/or its licensed developers and are protected by
  national and international copyright laws. They may not be disclosed to third
  parties or copied or duplicated in any form, in whole or in part, without the
  prior written consent of Nintendo.

  The content herein is highly confidential and should be handled accordingly.
 *--------------------------------------------------------------------------------*/

/**
* @examplesource{CodecOpusEncoder/CodecOpusEncoder.cpp,PageSampleCodecOpusEncoder}
*
* @brief
*  Opus encoder sample program.
*/

/**
* @page PageSampleCodecOpusEncoder  Opus Encoder
* @tableofcontents
*
* @brief
*  Description of the Opus decoder sample program.
*
* @section PageSampleCodecOpusEncoder_SectionBrief  Overview
*  This sample plays back audio while decoding with the Opus decoder any PCM
*  data that was encoded with the Opus encoder.
*  An audio renderer is used to play the audio.
*
* @section PageSampleCodecOpusEncoder_SectionFileStructure  File Structure
*  This sample program is located in <tt>@link Samples/Sources/Applications/CodecOpusEncoder Samples/Sources/Applications/CodecOpusEncoder/  @endlink </tt>.
*
*
* @section PageSampleCodecOpusEncoder_SectionNecessaryEnvironment  Required Environment
*  You must be able to use audio output.
*
* @section PageSampleCodecOpusEncoder_SectionHowToOperate  How to Use
*  Roughly a minute's worth of background music is played and the program ends automatically when you run the sample program.
*
* @section PageSampleCodecOpusEncoder_SectionPrecaution  Important Information
*  Nothing specific.
*
* @section PageSampleCodecOpusEncoder_SectionHowToExecute  Execution Procedure
*  Build the sample program and then run it.
*
* @section PageSampleCodecOpusEncoder_SectionDetail  Description
*
* @subsection PageSampleCodecOpusEncoder_SectionSampleProgram  Sample Program
*  This sample program uses the following source code.
*
*  <tt>CodecOpusEncoder.cpp</tt>
* @includelineno CodecOpusEncoder.cpp
*
* @subsection PageSampleCodecOpusEncoder_SectionSampleDetail  Sample Program Description
*
*  This sample program plays back the PCM data that results when decoding the Opus data created
*  when encoding PCM data.
*
*  This sample program has the following flow.
*
*  - Initialize the file system.
*  - Load PCM data.
*  - Initialize the Opus encoder.
*  - Initialize the Opus decoder.
*  - Initialize and start the audio renderer.
*  - Run the Opus encoder and decoder and play back the decoding results.
*  - Finalize the audio renderer.
*  - Finalize the Opus encoder.
*  - Finalize the Opus decoder.
*  - Finalize the file system.
*
*  First, initialize the file system.
*  It uses sound source data in the WAV format for resource files.
*  Change the resource directory as needed.
*
*  Loads the sound source data and the PCM data from a file.
*
*  The Opus encoder is initialized based on the header information.
*
*  You must pass the sample rate, number of channels, and a working buffer sized appropriately according to the previous two parameters to the <tt>nn::codec::OpusEncoder::Initialize()</tt> function.
*
*  Use the <tt>nn::codec::OpusEncoder::GetWorkBufferSize()</tt> function to get the working buffer size.
*
*  Initialize the Opus decoder.
*  For more information about <tt>nn::codec::OpusDecoder</tt>, see the API Reference and the <tt>Samples/Sources/Applications/OpusDecoder/</tt> sample.
*
*  Get the audio renderer ready.
*  For more information about audio renderers, see the API Reference and the <tt>Samples/Sources/Applications/AudioRenderer/</tt> sample.
*
*  The main loop implements a real-time streaming playback process.
*  The Opus decoder is used to decode PCM data that was encoded with the Opus encoder. This data may have been encoded by the distributor of the audio data. Decoded PCM data can be decoded by the downloader of the distributed audio data. PCM data is added to a buffer where playback has ended, and playback occurs consecutively using <tt>nn::audio::VoiceType</tt>.
*
*
*
*
*  Encodes all PCM data. Halts the audio renderer when playback ends and
*  finalizes the Opus encoder and decoder, and the audio renderer, and then destroys the memory.
*/

#include <cstdlib>  // std::malloc, std::free
#include <nn/nn_Abort.h>
#include <nn/nn_Assert.h>
#include <nn/audio.h>
#include <nn/codec.h>
#include <nn/fs.h>
#include <nn/mem.h>
#include <nn/os.h>

#include <nns/audio/audio_WavFormat.h>
#include <nns/nns_Log.h>

//==============================================================================
// Encode USE_SILK_CODING to encode in the Silk encryption mode.
//#define USE_SILK_CODING
//==============================================================================

#include "SwitchVoiceChatNativeCode.h"
#include "SwitchVoiceChatDecodeNativeCode.h"

namespace {
    const int MaxAudioIns = 2;

    char g_HeapBuffer[128 * 1024 * 16];
    const size_t ThreadStackSize = 8192 * 2;
    NN_ALIGNAS(4096) char g_AudioOutThreadStack[ThreadStackSize];
    nn::os::ThreadType    g_AudioOutThread;
    NN_ALIGNAS(4096) char g_AudioInThreadStack[ThreadStackSize];
    nn::os::ThreadType    g_AudioInThread;
    struct AudioOutArgs
    {
        nn::os::SystemEvent* event;
        nn::audio::AudioOut* audioOut;
        std::atomic<bool> isRunning;
    };
    AudioOutArgs g_OutArgs;

    void AudioOutThread(void* arg)
    {
        AudioOutArgs* args = reinterpret_cast<AudioOutArgs*>(arg);
        nn::audio::AudioOut* audioOutPointer = args->audioOut;
        intptr_t* handler = new intptr_t;
        char* something = new char;
        char** bufferOut = &something;
        int* count = new int();
        float* something2 = new float;
        float** bufferPtr = &something2;
        int* sampleCount;
        unsigned int* sampleRateOut = new unsigned int;
        while (args->isRunning)
        {
            // Get the buffer that completed recording.
            args->event->Wait();

            if (!args->isRunning)
            {
                break;
            }

            nn::audio::AudioOutBuffer* pAudioOutBuffer = nullptr;

            pAudioOutBuffer = nn::audio::GetReleasedAudioOutBuffer(audioOutPointer);

            int bufferCount = 0;
            while (pAudioOutBuffer)
            {
                ++bufferCount;
                // Copy data and register again.
                void* pOutBuffer = nn::audio::GetAudioOutBufferDataPointer(pAudioOutBuffer);
                char* pOutBufferChar = reinterpret_cast<char*>(pOutBuffer);
                size_t outSize = nn::audio::GetAudioOutBufferDataSize(pAudioOutBuffer);

                memset(pOutBuffer, 0, outSize);
                if (SwitchVoiceChatNativeCode::wntgd_GetVoiceBuffer(handler, bufferOut, count))
                {
                    int count2;
                    if (SwitchVoiceChatDecodeNativeCode::wntgd_DecompressVoiceData(handler, pOutBufferChar, *count, bufferPtr, count, sampleRateOut))
                    {
                        NN_LOG("AAAAAAAAAAAAAAAAAAAAAAAAAAAA");
                    }
                }
                nn::audio::AppendAudioOutBuffer(audioOutPointer, pAudioOutBuffer);
                pAudioOutBuffer = nn::audio::GetReleasedAudioOutBuffer(audioOutPointer);
            }
        }
    }

//char g_HeapBuffer[8 * 1024 * 1024];

char* g_MountRomCacheBuffer = nullptr;

const int BufferCount = 4;
const int OpusChannelCount = 2;
const int OpusSampleCount = 960;

NN_AUDIO_ALIGNAS_MEMORY_POOL_ALIGN int16_t g_PcmBuffer[1024 * 1024];
NN_STATIC_ASSERT(BufferCount * OpusSampleCount * OpusChannelCount * sizeof(int16_t) <= sizeof(g_PcmBuffer));

void InitializeFileSystem()
{
    size_t mountRomCacheBufferSize = 0;
    NN_ABORT_UNLESS_RESULT_SUCCESS(nn::fs::QueryMountRomCacheSize(&mountRomCacheBufferSize));
    g_MountRomCacheBuffer = new char[mountRomCacheBufferSize];

    NN_ABORT_UNLESS_RESULT_SUCCESS(nn::fs::MountRom("asset", g_MountRomCacheBuffer, mountRomCacheBufferSize));
}

void FinalizeFileSystem()
{
    nn::fs::Unmount("asset");

    delete[] g_MountRomCacheBuffer;
    g_MountRomCacheBuffer = nullptr;
}

}

extern "C" void nnMain()
{
#ifdef USE_SILK_CODING
    NNS_LOG("CodecOpusEncoder sample (using SILK coding)\n");
#else  // USE_SILK_CODING
    NNS_LOG("CodecOpusEncoder sample (using CELT coding)\n");
#endif  // USE_SILK_CODING

    nn::mem::StandardAllocator allocator(g_HeapBuffer, sizeof(g_HeapBuffer));
    nn::mem::StandardAllocator pcmBufferAllocator(g_PcmBuffer, sizeof(g_PcmBuffer));

    // Initialize the file system.
    InitializeFileSystem();

    // Load sound source data.
    const char Filename[] = "asset:/SampleBgm0-1ch.wav";
    nn::fs::FileHandle handle;
    NN_ABORT_UNLESS_RESULT_SUCCESS(nn::fs::OpenFile(&handle, Filename, nn::fs::OpenMode_Read));
    int64_t size;
    NN_ABORT_UNLESS_RESULT_SUCCESS(nn::fs::GetFileSize(&size, handle));
    auto const pWaveDataHead = new uint8_t[static_cast<size_t>(size)];
    NN_ABORT_UNLESS_NOT_NULL(pWaveDataHead);
    NN_ABORT_UNLESS_RESULT_SUCCESS(nn::fs::ReadFile(handle, 0, pWaveDataHead, static_cast<std::size_t>(size)));
    nn::fs::CloseFile(handle);

    const int8_t* pWaveData = reinterpret_cast<const int8_t*>(pWaveDataHead);

    // Get the sample rate and the number of channels from the header.
    nns::audio::WavFormat wavFormat;
    auto parseResult = ParseWavFormat(&wavFormat, pWaveDataHead, static_cast<size_t>(size));
    NN_ABORT_UNLESS(parseResult == nns::audio::WavResult_Success);
    const int wavChannelCount = wavFormat.channelCount;
    const int encoderSampleRate = wavFormat.sampleRate;
    const int64_t sampleCountPerChannel = wavFormat.dataSize / (wavFormat.bitsPerSample >> 3) / wavChannelCount;

    intptr_t* handler = new intptr_t;
    char* something = new char;
    char** bufferOut = &something;
    int* count = new int();
    float* something2 = new float;
    float** bufferPtr = &something2;
    int* sampleCount;
    unsigned int* sampleRateOut = new unsigned int;

    //Custom code
    SwitchVoiceChatNativeCode::wntgd_StartRecordVoice();
    SwitchVoiceChatDecodeNativeCode::wntgd_InitializeDecoder();

    nn::audio::AudioOutParameter audioOutParameter;
    audioOutParameter.channelCount = 1;
    nn::audio::AudioOut audioOut;

    auto argc = nn::os::GetHostArgc();
    auto argv = nn::os::GetHostArgv();
    int bufferLengthInMilliseconds = 50;
    if (argc >= 2)
    {
        bufferLengthInMilliseconds = atoi(argv[1]);
    }
    if (bufferLengthInMilliseconds > 50 || bufferLengthInMilliseconds < 0)
    {
        NNS_LOG("Clamping bufferLengthInMilliseconds from %d to 50\n", bufferLengthInMilliseconds);
        bufferLengthInMilliseconds = 50;
    }
    nn::os::SystemEvent audioOutSystemEvent;
    // Open audio input and audio output.

    nn::audio::InitializeAudioOutParameter(&audioOutParameter);
    NN_ABORT_UNLESS(
        nn::audio::OpenDefaultAudioOut(&audioOut, &audioOutSystemEvent, audioOutParameter).IsSuccess(),
        "Failed to open AudioOut."
    );

    int channelCount = nn::audio::GetAudioOutChannelCount(&audioOut);
    int sampleRate = nn::audio::GetAudioOutSampleRate(&audioOut);
    nn::audio::SampleFormat sampleFormat = nn::audio::GetAudioOutSampleFormat(&audioOut);
    // Prepare parameters for the buffer.
    NNS_LOG("bufferLengthInMilliseconds = %d\n", bufferLengthInMilliseconds);
    int millisecondsPerSecond = 1000;
    int frameRate = millisecondsPerSecond / bufferLengthInMilliseconds;
    int frameSampleCount = sampleRate / frameRate;
    size_t dataSize = frameSampleCount * channelCount * nn::audio::GetSampleByteSize(sampleFormat);
    size_t bufferSize = nn::util::align_up(dataSize, nn::audio::AudioOutBuffer::SizeGranularity);

    const int outBufferCount = 3;

    // Allocate and add a buffer.
    nn::audio::AudioOutBuffer audioOutBuffer[outBufferCount];
    void* outBuffer[outBufferCount];

    for (int i = 0; i < outBufferCount; ++i)
    {
        outBuffer[i] = allocator.Allocate(bufferSize, nn::audio::AudioOutBuffer::AddressAlignment);
        NN_ASSERT(outBuffer[i]);
        std::memset(outBuffer[i], 0, bufferSize);
        nn::audio::SetAudioOutBufferInfo(&audioOutBuffer[i], outBuffer[i], bufferSize, dataSize);
        nn::audio::AppendAudioOutBuffer(&audioOut, &audioOutBuffer[i]);
    }
    // Start recording and playback.
    NN_ABORT_UNLESS(
        nn::audio::StartAudioOut(&audioOut).IsSuccess(),
        "Failed to start AudioOut."
    );

    // Wait one frame.
    const nn::TimeSpan interval(nn::TimeSpan::FromNanoSeconds(1000 * 1000 * 1000 / frameRate));
    nn::os::SleepThread(interval);

    // Perform echo back for 15 seconds.

    g_OutArgs.event = &audioOutSystemEvent;
    g_OutArgs.audioOut = &audioOut;
    g_OutArgs.isRunning = true;

    nn::os::CreateThread(&g_AudioOutThread, AudioOutThread, &g_OutArgs, g_AudioOutThreadStack,
        ThreadStackSize, nn::os::HighestThreadPriority);

    nn::os::StartThread(&g_AudioOutThread);

    nn::audio::AudioOut* audioOutPointer = &audioOut;
    for(;;)
    {
        //// Get the buffer that completed recording.
        ////args->event->Wait();

        ///*if (!args->isRunning)
        //{
        //    break;
        //}*/

        //nn::audio::AudioOutBuffer* pAudioOutBuffer = nullptr;

        //pAudioOutBuffer = nn::audio::GetReleasedAudioOutBuffer(audioOutPointer);

        //int bufferCount = 0;
        //while (pAudioOutBuffer)
        //{
        //    ++bufferCount;
        //    // Copy data and register again.
        //    void* pOutBuffer = nn::audio::GetAudioOutBufferDataPointer(pAudioOutBuffer);
        //    char* pOutBufferChar = reinterpret_cast<char*>(pOutBuffer);
        //    size_t outSize = nn::audio::GetAudioOutBufferDataSize(pAudioOutBuffer);

        //    memset(pOutBuffer, 0, outSize);
        //    if (SwitchVoiceChatNativeCode::wntgd_GetVoiceBuffer(handler, bufferOut, count))
        //    {
        //        int count2;
        //        if (SwitchVoiceChatDecodeNativeCode::wntgd_DecompressVoiceData(handler, pOutBufferChar, *count, bufferPtr, count, sampleRateOut))
        //        {
        //            NN_LOG("AAAAAAAAAAAAAAAAAAAAAAAAAAAA");
        //        }
        //    }
        //    nn::audio::AppendAudioOutBuffer(audioOutPointer, pAudioOutBuffer);
        //    pAudioOutBuffer = nn::audio::GetReleasedAudioOutBuffer(audioOutPointer);
        //}
    }

    for (;;)
    {
        if (SwitchVoiceChatNativeCode::wntgd_GetVoiceBuffer(handler, bufferOut, count))
        {
            int count2;
            if (SwitchVoiceChatDecodeNativeCode::wntgd_DecompressVoiceData(handler, *bufferOut, *count, bufferPtr, count, sampleRateOut))
            {

            }
        }
    }

    // Initialize the Opus encoder.
    nn::codec::OpusEncoder encoder;
    void* encoderWorkBuffer = std::malloc(encoder.GetWorkBufferSize(encoderSampleRate, channelCount));
    NN_ABORT_UNLESS_NOT_NULL(encoderWorkBuffer);
    nn::codec::OpusResult opusResult = encoder.Initialize(encoderSampleRate, channelCount, encoderWorkBuffer, encoder.GetWorkBufferSize(encoderSampleRate, channelCount));
    NN_ABORT_UNLESS(opusResult == nn::codec::OpusResult_Success);

    // Set the bit rate of the encoder.
    const int HigherBitRate = 60000;
    const int LowerBitRate = 12000;
    encoder.SetBitRate(HigherBitRate);

    // Initialize the Opus decoder.
    const int decoderSampleRate = 48000;
    nn::codec::OpusDecoder decoder;
    void* decoderWorkBuffer = std::malloc(decoder.GetWorkBufferSize(decoderSampleRate, channelCount));
    NN_ABORT_UNLESS_NOT_NULL(decoderWorkBuffer);
    opusResult = decoder.Initialize(decoderSampleRate, channelCount, decoderWorkBuffer, decoder.GetWorkBufferSize(decoderSampleRate, channelCount));
    NN_ABORT_UNLESS(opusResult == nn::codec::OpusResult_Success);

    //Build the audio renderer.
    nn::audio::AudioRendererParameter audioRendererParameter;
    nn::audio::InitializeAudioRendererParameter(&audioRendererParameter);
    audioRendererParameter.sampleRate = decoderSampleRate;
    audioRendererParameter.sampleCount = 240;
    audioRendererParameter.mixBufferCount = 2;
    audioRendererParameter.voiceCount = 1;
    audioRendererParameter.sinkCount = 1;
    NN_ABORT_UNLESS(nn::audio::IsValidAudioRendererParameter(audioRendererParameter));

    std::size_t audioRendererWorkBufferSize = nn::audio::GetAudioRendererWorkBufferSize(audioRendererParameter);
    void* audioRendererWorkBuffer = allocator.Allocate(audioRendererWorkBufferSize, nn::os::MemoryPageSize);
    NN_ABORT_UNLESS_NOT_NULL(audioRendererWorkBuffer);

    nn::audio::AudioRendererHandle audioRendererHandle;
    nn::os::SystemEvent systemEvent;
    NN_ABORT_UNLESS_RESULT_SUCCESS(nn::audio::OpenAudioRenderer(&audioRendererHandle, &systemEvent, audioRendererParameter, audioRendererWorkBuffer, audioRendererWorkBufferSize));

    std::size_t configBufferSize = nn::audio::GetAudioRendererConfigWorkBufferSize(audioRendererParameter);
    void* configBuffer = allocator.Allocate(configBufferSize, nn::os::MemoryPageSize);
    NN_ABORT_UNLESS_NOT_NULL(configBuffer);
    nn::audio::AudioRendererConfig config;
    nn::audio::InitializeAudioRendererConfig(&config, audioRendererParameter, configBuffer, configBufferSize);

    nn::audio::FinalMixType finalMix;
    nn::audio::AcquireFinalMix(&config, &finalMix, 2);

    nn::audio::DeviceSinkType sink;
    int8_t bus[2] = { 0, 1 };
    NN_ABORT_UNLESS_RESULT_SUCCESS(nn::audio::AddDeviceSink(&config, &sink, &finalMix, bus, sizeof(bus) / sizeof(bus[0]), "MainAudioOut"));

    // Prepare a memory pool to maintain the sample data added to WaveBuffer.
    nn::audio::MemoryPoolType pcmBufferMemoryPool;
    NN_ABORT_UNLESS(nn::audio::AcquireMemoryPool(&config, &pcmBufferMemoryPool, g_PcmBuffer, sizeof(g_PcmBuffer)));
    NN_ABORT_UNLESS(nn::audio::RequestAttachMemoryPool(&pcmBufferMemoryPool));

    nn::audio::VoiceType voice;
    nn::audio::AcquireVoiceSlot(&config, &voice, decoderSampleRate, channelCount, nn::audio::SampleFormat_PcmInt16, nn::audio::VoiceType::PriorityHighest, nullptr, 0);
    nn::audio::SetVoiceDestination(&config, &voice, &finalMix);
    nn::audio::SetVoicePlayState(&voice, nn::audio::VoiceType::PlayState_Play);
    nn::audio::SetVoiceMixVolume(&voice, &finalMix, 1.0f, 0, 0);
    nn::audio::SetVoiceMixVolume(&voice, &finalMix, 1.0f, 0, 1);

    // Prepare buffers for streaming playback.
    // Pass in buffers initialized to 0, which get filled with the results of decoding the Opus data as part of the main loop.
    nn::audio::WaveBuffer waveBuffer[BufferCount];
    void* pcmBuffer[BufferCount];
    for (int i = 0; i < BufferCount; ++i)
    {
        const std::size_t pcmBufferSize = OpusSampleCount * OpusChannelCount * sizeof(int16_t);
        pcmBuffer[i] = pcmBufferAllocator.Allocate(pcmBufferSize, nn::audio::BufferAlignSize);

        std::memset(pcmBuffer[i], 0, pcmBufferSize);

        waveBuffer[i].buffer = pcmBuffer[i];
        waveBuffer[i].size = sizeof(g_PcmBuffer[i]);
        waveBuffer[i].startSampleOffset = 0;
        waveBuffer[i].endSampleOffset = static_cast<int32_t>(sizeof(g_PcmBuffer[i]) / sizeof(int16_t));
        waveBuffer[i].loop = false;
        waveBuffer[i].isEndOfStream = false;
        waveBuffer[i].pContext = nullptr;
        waveBuffer[i].contextSize = 0;

        nn::audio::AppendWaveBuffer(&voice, &waveBuffer[i]);
    }

    // Start the audio renderer.
    NN_ABORT_UNLESS_RESULT_SUCCESS(nn::audio::StartAudioRenderer(audioRendererHandle));

    // Get the PCM data itself.
    auto p = reinterpret_cast<const int16_t*>(pWaveData);
    int bufferCount = BufferCount;
    const size_t encodedDataSizeMaximum = nn::codec::OpusPacketSizeMaximum;

    // Frames can be changed at the encoding level.
    // Use encoding that switches, in a cycle, from 5 to 10 to 2.5 to 20 milliseconds.
    const int encodeFrameDurationCount = 4;
#if !defined(USE_SILK_CODING)
    const auto codingMode = nn::codec::OpusCodingMode_Celt;
    const int encodeFrameDurations[encodeFrameDurationCount] =
    {
//         // If the encodeFrameDuration value is set to less than 5000, note that the processing may
//         // not be performed in real time depending on the environment.
//          2500,
//          5000,
        10000,
        20000,
        10000,
        20000
    };
#else  // ! defined(USE_SILK_CODING)
    const auto codingMode = nn::codec::OpusCodingMode_Silk;
    const int encodeFrameDurations[encodeFrameDurationCount] =
    {
        10000,
        20000,
        10000,
        20000
    };
#endif // ! defined(USE_SILK_CODING)
    // You can specify an encryption mode before calling EncodeInterleaved().
    encoder.BindCodingMode(codingMode);

    const int encodeSampleCountMaximum = encoder.CalculateFrameSampleCount(20000);
    auto encodedDataBuffer = new uint8_t[encodedDataSizeMaximum];
    auto decodedDataBuffer = new int16_t[encodeSampleCountMaximum * channelCount];
    auto inputDataBuffer = new int16_t[encodeSampleCountMaximum * channelCount];
    int sampleCountTotal = 0;
    int encodeFrameDurationIndex = 0;

    for (auto remaining = sampleCountPerChannel; ; )
    {
        systemEvent.Wait();
        NN_ABORT_UNLESS_RESULT_SUCCESS(nn::audio::RequestUpdateAudioRenderer(audioRendererHandle, &config));

        // If playback is complete for any buffers, start the Opus decoding process.
        if (auto pReleasedWaveBuffer = nn::audio::GetReleasedWaveBuffer(&voice))
        {
            nn::audio::WaveBuffer* pWaveBuffer = nullptr;
            int16_t* pPcmBuffer = nullptr;
            for (int i = 0; i < BufferCount; ++i)
            {
                if (pReleasedWaveBuffer == &waveBuffer[i])
                {
                    pWaveBuffer = &waveBuffer[i];
                    pPcmBuffer = static_cast<int16_t*>(pcmBuffer[i]);
                    break;
                }
            }
            if (remaining > 0) // < sampleCountPerChannel / encodeSampleCount)
            {
                // You can switch the bit rate at the encoding level.
                // Switches between low and high bit rates for every 10,000
                // samples processed, for the following processing.
                if (sampleCountTotal >= 10000)
                {
                    int bitRate = encoder.GetBitRate();
                    if (HigherBitRate == bitRate)
                    {
                        encoder.SetBitRate(LowerBitRate);
                    }
                    else
                    {
                        encoder.SetBitRate(HigherBitRate);
                    }
                    sampleCountTotal -= 10000;
                }

                // Encoding.
                const int encodeSampleCount = encoder.CalculateFrameSampleCount(encodeFrameDurations[encodeFrameDurationIndex++]);
                encodeFrameDurationIndex = encodeFrameDurationIndex % encodeFrameDurationCount;
                size_t copySampleCount = encodeSampleCount;
                // Fractional processing.
                if (remaining < encodeSampleCount)
                {
                    copySampleCount = static_cast<int>(remaining);
                    std::memset(inputDataBuffer, 0x0, sizeof(int16_t) * channelCount * encodeSampleCount);
                }
                std::memcpy(inputDataBuffer, p, sizeof(int16_t) * channelCount * copySampleCount);
                size_t encodedDataSize = 0;
                nn::codec::OpusResult encodeStatus = encoder.EncodeInterleaved(
                    &encodedDataSize,
                    encodedDataBuffer,
                    encodedDataSizeMaximum,
                    inputDataBuffer,
                    encodeSampleCount);
                NN_ABORT_UNLESS(encodeStatus == nn::codec::OpusResult_Success);
                p += encodeSampleCount * channelCount;

                // Decoding
                std::size_t consumed = 0;
                int sampleCount = 0;
                const int decodeBufferSize = encodeSampleCount * channelCount * sizeof(int16_t) * decoderSampleRate / encoderSampleRate;
                opusResult = decoder.DecodeInterleaved(
                    &consumed,
                    &sampleCount,
                    pPcmBuffer,
                    decodeBufferSize,
                    encodedDataBuffer,
                    encodedDataSize);
                NN_ABORT_UNLESS(opusResult == nn::codec::OpusResult_Success);
                NN_ABORT_UNLESS(consumed == encodedDataSize);

                // Add decoded results to the Voice playback buffer.
                pWaveBuffer->size = sampleCount * sizeof(int16_t);
                pWaveBuffer->endSampleOffset = sampleCount;
                pWaveBuffer->loop = false;
                pWaveBuffer->isEndOfStream = false;
                nn::audio::AppendWaveBuffer(&voice, pWaveBuffer);

                remaining -= encodeSampleCount;
                sampleCountTotal += encodeSampleCount;

            }
            // If no data remains, wait until playback of all the buffers is complete.
            else if (--bufferCount == 0)
            {
                break;
            }
        }
    }

    // Release the buffers.
    for (int i = 0; i < BufferCount; ++i)
    {
        pcmBufferAllocator.Free(pcmBuffer[i]);
    }
    // Issue a request to detach the memory pools that were previously being used.
    nn::audio::RequestDetachMemoryPool(&pcmBufferMemoryPool);
    // Wait for the memory pools to be detached.
    while (nn::audio::IsMemoryPoolAttached(&pcmBufferMemoryPool))
    {
        nn::audio::GetReleasedWaveBuffer(&voice);

        systemEvent.Wait();
        NN_ABORT_UNLESS_RESULT_SUCCESS(nn::audio::RequestUpdateAudioRenderer(audioRendererHandle, &config));
    }
    // Release MemoryPool.
    nn::audio::ReleaseMemoryPool(&config, &pcmBufferMemoryPool);

    // Finalize the audio renderer, and free the memory.
    nn::audio::StopAudioRenderer(audioRendererHandle);
    nn::audio::CloseAudioRenderer(audioRendererHandle);
    nn::os::DestroySystemEvent(systemEvent.GetBase());
    allocator.Free(audioRendererWorkBuffer);
    allocator.Free(configBuffer);

    delete[] inputDataBuffer;
    delete[] decodedDataBuffer;
    delete[] encodedDataBuffer;
    delete[] pWaveDataHead;

    // Terminate the Opus decoder and encoder, and destroy the memory.
    encoder.Finalize();
    decoder.Finalize();
    std::free(encoderWorkBuffer);
    std::free(decoderWorkBuffer);

    // Finalize the file system.
    FinalizeFileSystem();

}  // NOLINT(readability/fn_size)
