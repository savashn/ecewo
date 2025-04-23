#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "request.h"

#define MAX_DYNAMIC_PARAMS 20

void parse_params(const char *path, const char *route_path, request_t *params)
{
    printf("Parsing dynamic params for path: %s, route: %s\n", path, route_path);
    params->count = 0;

    // Yolları '/' karakterinden ayırmak için geçici kopyalar oluştur
    char path_copy[256];
    char route_copy[256];
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';
    strncpy(route_copy, route_path, sizeof(route_copy) - 1);
    route_copy[sizeof(route_copy) - 1] = '\0';

    // Yol bölümlerini tutan diziler
    char *path_segments[20];
    char *route_segments[20];
    int path_segment_count = 0;
    int route_segment_count = 0;

    // Yol bölümlerini ayır
    char *token = strtok(path_copy, "/");
    while (token != NULL && path_segment_count < 20)
    {
        path_segments[path_segment_count++] = token;
        token = strtok(NULL, "/");
    }

    // Route bölümlerini ayır
    token = strtok(route_copy, "/");
    while (token != NULL && route_segment_count < 20)
    {
        route_segments[route_segment_count++] = token;
        token = strtok(NULL, "/");
    }

    // Segment sayıları farklıysa, route eşleşmeyebilir
    if (path_segment_count != route_segment_count)
    {
        printf("Warning: Path and route segment counts differ (%d vs %d)\n",
               path_segment_count, route_segment_count);
        // Yine de devam edelim, belki başlangıç segmentleri eşleşiyordur
    }

    // Bölümleri karşılaştır ve parametreleri çıkar
    int min_segments = path_segment_count < route_segment_count ? path_segment_count : route_segment_count;

    for (int i = 0; i < min_segments; i++)
    {
        // Eğer route segmenti ':' ile başlıyorsa, bu bir parametredir
        if (route_segments[i][0] == ':')
        {
            // Parametre adını al (':' karakterini atla)
            char *param_key = strdup(route_segments[i] + 1);
            // Parametre değerini al
            char *param_value = strdup(path_segments[i]);

            // Parametreyi ekle
            if (params->count < MAX_DYNAMIC_PARAMS)
            {
                params->items[params->count].key = param_key;
                params->items[params->count].value = param_value;
                params->count++;
                printf("Parsed parameter: %s = %s\n", param_key, param_value);
            }
            else
            {
                // Bellek sızıntısını önlemek için temizle
                free(param_key);
                free(param_value);
                printf("Error: Maximum parameter count reached\n");
                break;
            }
        }
        else if (strcmp(route_segments[i], path_segments[i]) != 0)
        {
            // Statik segment eşleşmiyorsa
            printf("Warning: Static segment mismatch at position %d: '%s' vs '%s'\n",
                   i, route_segments[i], path_segments[i]);
        }
    }
}
const char *params_get(request_t *params, const char *key)
{
    for (int i = 0; i < params->count; i++)
    {
        if (strcmp(params->items[i].key, key) == 0)
        {
            return params->items[i].value;
        }
    }
    return NULL;
}

void free_params(request_t *params)
{
    for (int i = 0; i < params->count; i++)
    {
        free(params->items[i].key);
        free(params->items[i].value);
    }
    free(params->items);
    params->count = 0;
}
