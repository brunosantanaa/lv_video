/**
 * avi_info.c - Programa para extrair e exibir metadados de arquivos AVI
 *
 * Compilação: gcc avi_info.c -o avi_info
 * Uso: ./avi_info caminho/para/arquivo.avi
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>

#define FOURCC_SIZE 4
#define MAX_STREAMS 10

typedef struct
{
    char id[FOURCC_SIZE];
    uint32_t size;
} ChunkHeader;

typedef struct
{
    uint32_t micro_sec_per_frame;
    uint32_t max_bytes_per_sec;
    uint32_t padding_granularity;
    uint32_t flags;
    uint32_t total_frames;
    uint32_t initial_frames;
    uint32_t streams;
    uint32_t suggested_buffer_size;
    uint32_t width;
    uint32_t height;
    uint32_t reserved[4];
} AVIMainHeader;

typedef struct
{
    char type[FOURCC_SIZE];
    char handler[FOURCC_SIZE];
    uint32_t flags;
    uint32_t priority;
    uint32_t initial_frames;
    uint32_t scale;
    uint32_t rate;
    uint32_t start;
    uint32_t length;
    uint32_t suggested_buffer_size;
    uint32_t quality;
    uint32_t sample_size;
    struct
    {
        uint16_t left;
        uint16_t top;
        uint16_t right;
        uint16_t bottom;
    } frame;
} AVIStreamHeader;

typedef struct
{
    uint32_t size;
    uint32_t width;
    uint32_t height;
    uint16_t planes;
    uint16_t bit_count;
    uint32_t compression;
    uint32_t image_size;
    uint32_t x_pels_per_meter;
    uint32_t y_pels_per_meter;
    uint32_t clr_used;
    uint32_t clr_important;
} BitmapInfoHeader;

typedef struct
{
    uint16_t format_tag;
    uint16_t channels;
    uint32_t samples_per_sec;
    uint32_t avg_bytes_per_sec;
    uint16_t block_align;
    uint16_t bits_per_sample;
    uint16_t size;
} WaveFormatEx;

// Estrutura para armazenar informações de cada stream
typedef struct
{
    char type[FOURCC_SIZE + 1]; // +1 para o terminador nulo
    AVIStreamHeader header;
    union
    {
        BitmapInfoHeader video;
        WaveFormatEx audio;
    } format;
} StreamInfo;

// Estrutura para armazenar todos os metadados importantes do AVI
typedef struct
{
    AVIMainHeader main_header;
    uint32_t stream_count;
    StreamInfo streams[MAX_STREAMS];
    uint32_t video_streams;
    uint32_t audio_streams;
    char video_codec[5];
    uint32_t movi_offset; // Offset onde começa o 'movi' chunk (dados)
    uint32_t movi_size;   // Tamanho do chunk 'movi'
} AVIInfo;

// Função auxiliar para ler um inteiro de 32 bits little-endian
uint32_t read_uint32(FILE *file)
{
    uint8_t buf[4];
    fread(buf, 1, 4, file);
    return buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
}

// Função auxiliar para ler um inteiro de 16 bits little-endian
uint16_t read_uint16(FILE *file)
{
    uint8_t buf[2];
    fread(buf, 1, 2, file);
    return buf[0] | (buf[1] << 8);
}

// Função para ler um FourCC
void read_fourcc(FILE *file, char *fourcc)
{
    fread(fourcc, 1, FOURCC_SIZE, file);
}

// Função para verificar se dois FourCCs são iguais
int fourcc_equals(const char *a, const char *b)
{
    return memcmp(a, b, FOURCC_SIZE) == 0;
}

// Função para ler um chunk header
void read_chunk_header(FILE *file, ChunkHeader *header)
{
    read_fourcc(file, header->id);
    header->size = read_uint32(file);
}

// Função para ler o cabeçalho principal do AVI
void read_avi_main_header(FILE *file, AVIMainHeader *header)
{
    header->micro_sec_per_frame = read_uint32(file);
    header->max_bytes_per_sec = read_uint32(file);
    header->padding_granularity = read_uint32(file);
    header->flags = read_uint32(file);
    header->total_frames = read_uint32(file);
    header->initial_frames = read_uint32(file);
    header->streams = read_uint32(file);
    header->suggested_buffer_size = read_uint32(file);
    header->width = read_uint32(file);
    header->height = read_uint32(file);

    // Lê os valores reservados
    for (int i = 0; i < 4; i++)
    {
        header->reserved[i] = read_uint32(file);
    }
}

// Função para ler o cabeçalho de um stream
void read_stream_header(FILE *file, AVIStreamHeader *header)
{
    read_fourcc(file, header->type);
    read_fourcc(file, header->handler);
    header->flags = read_uint32(file);
    header->priority = read_uint32(file);
    header->initial_frames = read_uint32(file);
    header->scale = read_uint32(file);
    header->rate = read_uint32(file);
    header->start = read_uint32(file);
    header->length = read_uint32(file);
    header->suggested_buffer_size = read_uint32(file);
    header->quality = read_uint32(file);
    header->sample_size = read_uint32(file);

    // Lê o retângulo do frame
    header->frame.left = read_uint16(file);
    header->frame.top = read_uint16(file);
    header->frame.right = read_uint16(file);
    header->frame.bottom = read_uint16(file);
}

// Função para ler o formato de vídeo
void read_bitmap_info_header(FILE *file, BitmapInfoHeader *header)
{
    header->size = read_uint32(file);
    header->width = read_uint32(file);
    header->height = read_uint32(file);
    header->planes = read_uint16(file);
    header->bit_count = read_uint16(file);
    header->compression = read_uint32(file);
    header->image_size = read_uint32(file);
    header->x_pels_per_meter = read_uint32(file);
    header->y_pels_per_meter = read_uint32(file);
    header->clr_used = read_uint32(file);
    header->clr_important = read_uint32(file);
}

// Função para ler o formato de áudio
void read_wave_format_ex(FILE *file, WaveFormatEx *header)
{
    header->format_tag = read_uint16(file);
    header->channels = read_uint16(file);
    header->samples_per_sec = read_uint32(file);
    header->avg_bytes_per_sec = read_uint32(file);
    header->block_align = read_uint16(file);
    header->bits_per_sample = read_uint16(file);

    // Alguns arquivos WAVE têm informações adicionais
    if (ftell(file) % 2 != 0)
    {
        // Pula 1 byte para alinhamento
        fseek(file, 1, SEEK_CUR);
    }

    // Tenta ler o tamanho extra, se houver
    if (!feof(file))
    {
        header->size = read_uint16(file);
    }
    else
    {
        header->size = 0;
    }
}

// Função para extrair codec de vídeo do FOURCC
void get_codec_name(uint32_t fourcc, char *codec)
{
    memcpy(codec, &fourcc, 4);
    codec[4] = '\0';

    // Verifica se todos os caracteres são imprimíveis
    for (int i = 0; i < 4; i++)
    {
        if (codec[i] < 32 || codec[i] > 126)
        {
            codec[i] = '.';
        }
    }
}

// Função para obter informações do arquivo AVI
int parse_avi_file(const char *filename, AVIInfo *info)
{
    FILE *file = fopen(filename, "rb");
    if (!file)
    {
        printf("Erro ao abrir o arquivo: %s\n", filename);
        return -1;
    }

    // Inicializa a estrutura de informações
    memset(info, 0, sizeof(AVIInfo));

    // Lê o cabeçalho RIFF
    ChunkHeader riff_header;
    read_chunk_header(file, &riff_header);

    // Verifica se é um arquivo RIFF AVI válido
    if (!fourcc_equals(riff_header.id, "RIFF"))
    {
        printf("Não é um arquivo RIFF válido\n");
        fclose(file);
        return -1;
    }

    // Lê o tipo do RIFF
    char form_type[FOURCC_SIZE];
    read_fourcc(file, form_type);

    if (!fourcc_equals(form_type, "AVI "))
    {
        printf("Não é um arquivo AVI válido\n");
        fclose(file);
        return -1;
    }

    // Loop para processar os chunks principais
    long file_size = ftell(file);
    while (file_size < riff_header.size + 8)
    {
        ChunkHeader chunk;
        long chunk_start = ftell(file);
        read_chunk_header(file, &chunk);

        // Processa o chunk 'LIST'
        if (fourcc_equals(chunk.id, "LIST"))
        {
            // Lê o tipo da lista
            char list_type[FOURCC_SIZE];
            read_fourcc(file, list_type);

            // Processa o cabeçalho (hdrl)
            if (fourcc_equals(list_type, "hdrl"))
            {
                // Procura o chunk 'avih' (cabeçalho AVI)
                while (ftell(file) < chunk_start + chunk.size + 8)
                {
                    ChunkHeader sub_chunk;
                    long sub_chunk_start = ftell(file);
                    read_chunk_header(file, &sub_chunk);

                    if (fourcc_equals(sub_chunk.id, "avih"))
                    {
                        // Lê o cabeçalho principal do AVI
                        read_avi_main_header(file, &info->main_header);
                    }
                    else if (fourcc_equals(sub_chunk.id, "LIST"))
                    {
                        // Pode ser um 'strl' (stream list)
                        char sub_list_type[FOURCC_SIZE];
                        read_fourcc(file, sub_list_type);

                        if (fourcc_equals(sub_list_type, "strl") && info->stream_count < MAX_STREAMS)
                        {
                            // Processa informações do stream
                            StreamInfo *stream = &info->streams[info->stream_count];

                            // Procura chunks 'strh' e 'strf'
                            while (ftell(file) < sub_chunk_start + sub_chunk.size + 8)
                            {
                                ChunkHeader strl_chunk;
                                long strl_chunk_start = ftell(file);
                                read_chunk_header(file, &strl_chunk);

                                if (fourcc_equals(strl_chunk.id, "strh"))
                                {
                                    // Cabeçalho do stream
                                    read_stream_header(file, &stream->header);

                                    // Copia o tipo do stream
                                    memcpy(stream->type, stream->header.type, FOURCC_SIZE);
                                    stream->type[FOURCC_SIZE] = '\0';

                                    // Conta o tipo de stream
                                    if (fourcc_equals(stream->header.type, "vids"))
                                    {
                                        info->video_streams++;
                                        // Salva o codec de vídeo
                                        get_codec_name(*(uint32_t *)stream->header.handler, info->video_codec);
                                    }
                                    else if (fourcc_equals(stream->header.type, "auds"))
                                    {
                                        info->audio_streams++;
                                    }
                                }
                                else if (fourcc_equals(strl_chunk.id, "strf"))
                                {
                                    // Formato do stream
                                    if (fourcc_equals(stream->header.type, "vids"))
                                    {
                                        // Formato de vídeo (BITMAPINFOHEADER)
                                        read_bitmap_info_header(file, &stream->format.video);
                                    }
                                    else if (fourcc_equals(stream->header.type, "auds"))
                                    {
                                        // Formato de áudio (WAVEFORMATEX)
                                        read_wave_format_ex(file, &stream->format.audio);
                                    }
                                    else
                                    {
                                        // Stream desconhecido, pula
                                        fseek(file, strl_chunk.size - 8, SEEK_CUR);
                                    }
                                }
                                else
                                {
                                    // Chunk desconhecido, pula
                                    fseek(file, strl_chunk.size, SEEK_CUR);
                                }

                                // Alinhamento a 2 bytes
                                if (strl_chunk.size % 2 != 0)
                                {
                                    fseek(file, 1, SEEK_CUR);
                                }
                            }

                            info->stream_count++;
                        }
                        else
                        {
                            // Pula a lista desconhecida
                            fseek(file, sub_chunk.size - 4, SEEK_CUR);
                        }
                    }
                    else
                    {
                        // Chunk desconhecido, pula
                        fseek(file, sub_chunk.size, SEEK_CUR);
                    }

                    // Alinhamento a 2 bytes
                    if (sub_chunk.size % 2 != 0)
                    {
                        fseek(file, 1, SEEK_CUR);
                    }
                }
            }
            else if (fourcc_equals(list_type, "movi"))
            {
                // Anota o offset e o tamanho dos dados de mídia
                info->movi_offset = ftell(file) - 4; // -4 para voltar para o começo do 'movi'
                info->movi_size = chunk.size - 4;    // -4 para excluir o FourCC do tipo

                // Pula o chunk movi
                fseek(file, chunk.size - 4, SEEK_CUR);
            }
            else
            {
                // Lista desconhecida, pula
                fseek(file, chunk.size - 4, SEEK_CUR);
            }
        }
        else
        {
            // Chunk desconhecido, pula
            fseek(file, chunk.size, SEEK_CUR);
        }

        // Alinhamento a 2 bytes
        if (chunk.size % 2 != 0)
        {
            fseek(file, 1, SEEK_CUR);
        }

        file_size = ftell(file);
    }

    fclose(file);
    return 0;
}

// Função para imprimir informações sobre o arquivo AVI
void print_avi_info(const AVIInfo *info)
{
    printf("=== Informações do Arquivo AVI ===\n");
    printf("Dimensões: %dx%d pixels\n", info->main_header.width, info->main_header.height);

    // Calcula a duração em segundos e formata
    double fps = 0;
    if (info->main_header.micro_sec_per_frame > 0)
    {
        fps = 1000000.0 / info->main_header.micro_sec_per_frame;
    }

    double duration = 0;
    if (fps > 0)
    {
        duration = info->main_header.total_frames / fps;
    }

    int hours = (int)(duration / 3600);
    int minutes = (int)((duration - hours * 3600) / 60);
    double seconds = duration - hours * 3600 - minutes * 60;

    printf("Total de frames: %u\n", info->main_header.total_frames);
    printf("Taxa de frames: %.3f fps\n", fps);
    printf("Duração: %02d:%02d:%06.3f\n", hours, minutes, seconds);

    printf("\n-- Streams --\n");
    printf("Número total de streams: %u\n", info->stream_count);
    printf("Streams de vídeo: %u\n", info->video_streams);
    printf("Streams de áudio: %u\n", info->audio_streams);

    if (info->video_streams > 0)
    {
        printf("\n-- Informações de Vídeo --\n");
        for (uint32_t i = 0; i < info->stream_count; i++)
        {
            if (fourcc_equals(info->streams[i].header.type, "vids"))
            {
                printf("Stream %u:\n", i);
                printf("  Codec: %s\n", info->video_codec);
                printf("  Resolução: %ux%u\n",
                       info->streams[i].format.video.width,
                       info->streams[i].format.video.height);
                printf("  Bits por pixel: %u\n", info->streams[i].format.video.bit_count);
                printf("  Frames: %u\n", info->streams[i].header.length);

                double stream_fps = 0;
                if (info->streams[i].header.scale > 0)
                {
                    stream_fps = (double)info->streams[i].header.rate / info->streams[i].header.scale;
                }
                printf("  Taxa de frames: %.3f fps\n", stream_fps);
            }
        }
    }

    if (info->audio_streams > 0)
    {
        printf("\n-- Informações de Áudio --\n");
        for (uint32_t i = 0; i < info->stream_count; i++)
        {
            if (fourcc_equals(info->streams[i].header.type, "auds"))
            {
                printf("Stream %u:\n", i);
                printf("  Format tag: 0x%04X\n", info->streams[i].format.audio.format_tag);
                printf("  Canais: %u\n", info->streams[i].format.audio.channels);
                printf("  Samples por segundo: %u Hz\n", info->streams[i].format.audio.samples_per_sec);
                printf("  Bits por sample: %u\n", info->streams[i].format.audio.bits_per_sample);
                printf("  Bytes por segundo: %u\n", info->streams[i].format.audio.avg_bytes_per_sec);
            }
        }
    }

    printf("\n-- Tamanhos --\n");
    printf("Tamanho de buffer sugerido: %u bytes\n", info->main_header.suggested_buffer_size);
    printf("Taxa máxima de dados: %u bytes/s\n", info->main_header.max_bytes_per_sec);
    printf("Tamanho dos dados (movi): %u bytes\n", info->movi_size);
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("Uso: %s <caminho/para/arquivo.avi>\n", argv[0]);
        return 1;
    }

    AVIInfo info;
    if (parse_avi_file(argv[1], &info) == 0)
    {
        print_avi_info(&info);
        return 0;
    }

    return 1;
}