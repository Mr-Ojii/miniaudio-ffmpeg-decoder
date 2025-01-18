#define MA_IMPLEMENTATION
#include "miniaudio.h"
#include "miniaudio_ffmpeg.h"

static ma_result ma_decoding_backend_init__ffmpeg(void* pUserData, ma_read_proc onRead, ma_seek_proc onSeek, ma_tell_proc onTell, void* pReadSeekTellUserData, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_data_source** ppBackend)
{
    ma_result result;
    ma_ffmpeg* pFFmpeg;

    (void)pUserData;

    pFFmpeg = (ma_ffmpeg*)ma_malloc(sizeof(*pFFmpeg), pAllocationCallbacks);
    if (pFFmpeg == NULL) {
        return MA_OUT_OF_MEMORY;
    }

    result = ma_ffmpeg_init(onRead, onSeek, onTell, pReadSeekTellUserData, pConfig, pAllocationCallbacks, pFFmpeg);
    if (result != MA_SUCCESS) {
        ma_free(pFFmpeg, pAllocationCallbacks);
        return result;
    }

    *ppBackend = pFFmpeg;

    return MA_SUCCESS;
}

static void ma_decoding_backend_uninit__ffmpeg(void* pUserData, ma_data_source* pBackend, const ma_allocation_callbacks* pAllocationCallbacks)
{
    ma_ffmpeg* pFFmpeg = (ma_ffmpeg*)pBackend;

    (void)pUserData;

    ma_ffmpeg_uninit(pFFmpeg, pAllocationCallbacks);
    ma_free(pFFmpeg, pAllocationCallbacks);
}

static ma_decoding_backend_vtable g_ma_decoding_backend_vtable_ffmpeg =
{
    ma_decoding_backend_init__ffmpeg,
    NULL, /* onInitFile() */
    NULL, /* onInitFileW() */
    NULL, /* onInitMemory() */
    ma_decoding_backend_uninit__ffmpeg
};

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    ma_data_source* pDataSource = (ma_data_source*)pDevice->pUserData;
    if (pDataSource == NULL) {
        return;
    }

    ma_data_source_read_pcm_frames(pDataSource, pOutput, frameCount, NULL);

    (void)pInput;
}

int main(int argc, char** argv)
{
    ma_result result;
    ma_decoder_config decoderConfig;
    ma_decoder decoder;
    ma_device_config deviceConfig;
    ma_device device;
    ma_format format;
    ma_uint32 channels;
    ma_uint32 sampleRate;

    /*
    Add your custom backend vtables here. The order in the array defines the order of priority. The
    vtables will be passed in via the decoder config.
    */
    ma_decoding_backend_vtable* pCustomBackendVTables[] =
    {
        &g_ma_decoding_backend_vtable_ffmpeg,
    };


    if (argc < 2) {
        printf("No input file.\n");
        return -1;
    }

    
    /* Initialize the decoder. */
    decoderConfig = ma_decoder_config_init_default();
    decoderConfig.pCustomBackendUserData = NULL;  /* In this example our backend objects are contained within a ma_decoder_ex object to avoid a malloc. Our vtables need to know about this. */
    decoderConfig.ppCustomBackendVTables = pCustomBackendVTables;
    decoderConfig.customBackendCount     = sizeof(pCustomBackendVTables) / sizeof(pCustomBackendVTables[0]);
    
    result = ma_decoder_init_file(argv[1], &decoderConfig, &decoder);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize decoder. (%d)\n", result);
        return -1;
    }

    ma_data_source_set_looping(&decoder, MA_TRUE);


    /* Initialize the device. */
    result = ma_data_source_get_data_format(&decoder, &format, &channels, &sampleRate, NULL, 0);
    if (result != MA_SUCCESS) {
        printf("Failed to retrieve decoder data format. (%d)\n", result);
        ma_decoder_uninit(&decoder);
        return -1;
    }

    deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format   = format;
    deviceConfig.playback.channels = channels;
    deviceConfig.sampleRate        = sampleRate;
    deviceConfig.dataCallback      = data_callback;
    deviceConfig.pUserData         = &decoder;

    result = ma_device_init(NULL, &deviceConfig, &device);
    if (result != MA_SUCCESS) {
        printf("Failed to open playback device. (%d)\n", result);
        ma_decoder_uninit(&decoder);
        return -1;
    }

    result = ma_device_start(&device);
    if (result != MA_SUCCESS) {
        printf("Failed to start playback device. (%d)\n", result);
        ma_device_uninit(&device);
        ma_decoder_uninit(&decoder);
        return -1;
    }

    printf("Press Enter to quit...");
    getchar();

    ma_device_uninit(&device);
    ma_decoder_uninit(&decoder);

    return 0;
}
