#ifndef CORE_H
#define CORE_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#if defined(_WIN32)

#define PLATFORM_WINDOWS

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#elif defined(__linux__)

#define PLATFORM_LINUX
#define PLATFORM_POSIX

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#else

#error "Unsupported platform";

#endif

#define Alloc(T) memset(malloc(sizeof(T)), 0, sizeof(T))

#define StaticArraySize(arr) (sizeof(arr) / sizeof(*(arr)))

typedef struct {
    int64_t capacity;
    int64_t count;
    void **data;
} Array;

#define foreach(i, array) for (int64_t i = 0; i < (array).count; i += 1)
#define rev_foreach(i, array) for (int64_t i = array.count - 1; i >= 0; i -= 1)

static inline
void *ArrayGet(const Array array, int64_t i) {
    assert(i >= 0 && i < array.count && "Array bounds check failed");

    return array.data[i];
}

static inline
void ArrayReserve(Array *array, int64_t capacity) {
    if (capacity <= array->capacity) {
        return;
    }

    array->data = realloc(array->data, capacity * sizeof(void *));
    array->capacity = capacity;
}

static inline
void ArrayPush(Array *array, void *elem) {
    if (array->count >= array->capacity) {
        ArrayReserve(array, array->capacity * 2 + 8);
    }

    array->data[array->count] = elem;
    array->count += 1;
}

static inline
void ArrayRemove(Array *array, int64_t index) {
    assert(index >= 0 && index < array->count && "Array bounds check failed");

    array->data[index] = array->data[array->count - 1];
    array->count -= 1;
}

static inline
void ArrayOrderedInsert(Array *array, void *elem, int64_t index) {
    assert(index >= 0 && index <= array->count && "Array bounds check failed");

    if (array->count >= array->capacity) {
        ArrayReserve(array, array->capacity * 2 + 8);
    }

    for (int64_t i = array->count; i > index; i -= 1) {
        array->data[i] = array->data[i - 1];
    }

    array->count += 1;

    array->data[index] = elem;
}

static
void ArraySortHelper(Array *array, int64_t left, int64_t right, int (*compare)(void *a, void *b)) {
    if (left < 0 || right <= left) {
        return;
    }

    int64_t i = left;
    int64_t j = right;

    void *pivot = array->data[(j + i) / 2];

    while (true) {
        while (compare(array->data[i], pivot) < 0) {
            i += 1;
        }
        while (compare(array->data[j], pivot) > 0) {
            j -= 1;
        }

        if (i >= j) {
            break;
        }

        void *tmp = array->data[i];
        array->data[i] = array->data[j];
        array->data[j] = tmp;

        i += 1;
        j -= 1;
    }

    ArraySortHelper(array, left, j, compare);
    ArraySortHelper(array, j + 1, right, compare);
}

static inline
void ArraySort(Array *array, int (*compare)(void *a, void *b)) {
    if (array->count < 2) {
        return;
    }

    ArraySortHelper(array, 0, array->count - 1, compare);
}

typedef uint64_t (*HashMapHashFunc)(void *ptr);
typedef bool (*HashMapCompareFunc)(void *a, void *b);

// FNV-1a hash: http://www.isthe.com/chongo/tech/comp/fnv/index.html
#define FNV_64_Prime       0x100000001b3
#define FNV_64_Offset_Bias 0xcbf29ce484222325

static
uint64_t Fnv1aHashBase(void *ptr, int64_t size, uint64_t hash) {
    int64_t i = 0;
    while (i < size) {
        hash ^= (uint64_t)(((char *)ptr)[i]);
        hash *= FNV_64_Prime;
        i += 1;
    }

    return hash;
}

static inline
uint64_t Fnv1aHash(void *ptr, int64_t size) {
    return Fnv1aHashBase(ptr, size, FNV_64_Offset_Bias);
}

#define Fnv1aHashT(val) Fnv1aHash(&(val), sizeof(val))
#define Fnv1aHashBaseT(val, hash) Fnv1aHashBase(&(val), sizeof(val), hash)

static
uint64_t HashStringBase(char *str, uint64_t hash) {
    int64_t i = 0;
    while (str[i]) {
        hash ^= (uint64_t)(str[i]);
        hash *= FNV_64_Prime;
        i += 1;
    }

    return hash;
}

static inline
uint64_t HashString(char *str) {
    return HashStringBase(str, FNV_64_Offset_Bias);
}

#define Hash_Map_Never_Occupied 0
#define Hash_Map_Removed 1
#define Hash_Map_First_Occupied 1
#define Hash_Map_Min_Capacity 32
#define Hash_Map_Load_Limit 70

typedef struct {
    uint64_t hash;
    void *key;
    void *value;
} HashMapEntry;

typedef struct {
    int64_t capacity;
    int64_t occupied;
    int64_t count;
    HashMapEntry *entries;
    HashMapHashFunc hash_func;
    HashMapCompareFunc compare_func;
} HashMap;

static inline
HashMapEntry *HashMapNextEntry(HashMap map, HashMapEntry *curr) {
    int64_t i = 0;
    if (curr) {
        i = (int64_t)(curr - map.entries) + 1;
    }

    while (i >= 0 && i < map.capacity) {
        if (map.entries[i].hash >= Hash_Map_First_Occupied) {
            return &map.entries[i];
        }

        i += 1;
    }

    return NULL;
}

static inline
HashMapEntry *HashMapFirstEntry(HashMap map) {
    return HashMapNextEntry(map, NULL);
}

#define foreach_key_value(kv, map) for (HashMapEntry *kv = HashMapFirstEntry((map)); kv != NULL; kv = HashMapNextEntry((map), kv))

static inline
void HashMapInsert(HashMap *map, void *key, void *value);

static inline
void HashMapGrow(HashMap *map) {
    int64_t old_capacity = map->capacity;
    HashMapEntry *old_entries = map->entries;

    map->capacity = map->capacity * 2 > Hash_Map_Min_Capacity ? map->capacity * 2 : Hash_Map_Min_Capacity;
    map->entries = malloc(sizeof(HashMapEntry) * map->capacity);
    map->count = 0;
    map->occupied = 0;

    for (int64_t i = 0; i < map->capacity; i += 1) {
        map->entries[i].hash = Hash_Map_Never_Occupied;
    }

    for (int64_t i = 0; i < old_capacity; i += 1) {
        HashMapEntry entry = old_entries[i];
        if (entry.hash >= Hash_Map_First_Occupied) {
            HashMapInsert(map, entry.key, entry.value);
        }
    }
}

typedef struct {
    uint64_t hash;
    int64_t index;
    bool is_present;
} HashMapProbeResult;

static
HashMapProbeResult HashMapProbe(HashMap *map, void *key) {
    assert(map->capacity > 0);
    assert(map->compare_func != NULL);
    assert(map->hash_func != NULL);

    uint64_t mask = (uint64_t)(map->capacity - 1);
    uint64_t hash = map->hash_func(key);
    if (hash < Hash_Map_First_Occupied) {
        hash += Hash_Map_First_Occupied;
    }

    uint64_t index = hash & mask;
    uint64_t increment = 1 + (hash >> 27);
    while (map->entries[index].hash != Hash_Map_Never_Occupied) {
        HashMapEntry entry = map->entries[index];

        if (entry.hash == hash && map->compare_func(entry.key, key)) {
            return (HashMapProbeResult){hash, (int64_t)index, true};
        }

        index += increment;
        index &= mask;
        increment += 1;
    }

    return (HashMapProbeResult){hash, (int64_t)index, false};
}

static
void **HashMapFindOrAdd(HashMap *map, void *key, bool *was_present) {
    if ((map->occupied + 1) * 100 >= map->capacity * Hash_Map_Load_Limit) {
        HashMapGrow(map);
    }

    HashMapProbeResult probe = HashMapProbe(map, key);
    HashMapEntry *entry = &map->entries[probe.index];
    if (was_present) {
        *was_present = probe.is_present;
    }

    if (probe.is_present) {
        return &entry->value;
    }

    *entry = (HashMapEntry){0};
    entry->hash = probe.hash;
    entry->key = key;
    map->occupied += 1;
    map->count += 1;

    return &entry->value;
}

static inline
void HashMapInsert(HashMap *map, void *key, void *value) {
    void **ptr = HashMapFindOrAdd(map, key, NULL);
    *ptr = value;
}

static inline
void **HashMapFindPtr(HashMap *map, void *key) {
    if (map->count <= 0) {
        return NULL;
    }

    HashMapProbeResult probe = HashMapProbe(map, key);
    if (probe.is_present) {
        return &map->entries[probe.index].value;
    }

    return NULL;
}

static inline
void *HashMapFind(HashMap *map, void *key, void *fallback) {
    void **ptr = HashMapFindPtr(map, key);
    if (!ptr) {
        return fallback;
    }

    return *ptr;
}

static
bool HashMapRemove(HashMap *map, void *key, void **removed_value) {
    if (map->count <= 0) {
        if (removed_value) {
            *removed_value = NULL;
        }

        return false;
    }

    HashMapProbeResult probe = HashMapProbe(map, key);
    if (!probe.is_present) {
        if (removed_value) {
            *removed_value = NULL;
        }

        return false;
    }

    HashMapEntry *entry = &map->entries[probe.index];
    entry->hash = Hash_Map_Removed;
    map->count -= 1;

    if (removed_value) {
        *removed_value = entry->value;
    }

    return true;
}

static inline
bool StrEq(const char *a, const char *b) {
    if (!a || !b) {
        return false;
    }

    return strcmp(a, b) == 0;
}

static inline
char *StrJoin(const char *a, const char *b) {
    int64_t a_len = strlen(a);
    int64_t b_len = strlen(b);

    char *result = malloc(a_len + b_len + 1);

    memcpy(result, a, a_len);
    memcpy(result + a_len, b, b_len);
    result[a_len + b_len] = 0;

    return result;
}

#if defined(PLATFORM_WINDOWS)

static inline
char *strndup(const char *str, size_t size) {
    char *result = malloc(size + 1);

    size_t i = 0;
    while (str[i] && i < size) {
        result[i] = str[i];
        i += 1;
    }

    result[i] = 0;

    return result;
}

#endif

static inline
bool StrStartsWith(const char *a, const char *b) {
    if (!a || !b) {
        return false;
    }

    int64_t a_len = strlen(a);
    int64_t b_len = strlen(b);
    if (a_len < b_len) {
        return false;
    }

    return strncmp(a, b, b_len) == 0;
}

static inline
bool StrEndsWith(const char *a, const char *b) {
    if (!a || !b) {
        return false;
    }

    int64_t a_len = strlen(a);
    int64_t b_len = strlen(b);
    if (a_len < b_len) {
        return false;
    }

    return strcmp(a + a_len - b_len, b) == 0;
}

static inline
uint64_t StringHashFunc(void *str) {
    return HashString(str);
}

static inline
bool StringCompareFunc(void *a, void *b) {
    return StrEq(a, b);
}

static inline
char *PathJoin(const char *a, const char *b) {
    int64_t a_len = strlen(a);
    int64_t b_len = strlen(b);
    char *buffer = malloc(a_len + b_len + 2);

    int64_t len = 0;
    memcpy(buffer, a, a_len);
    len += a_len;

    if (a_len > 0 && a[a_len - 1] != '/') {
        buffer[len] = '/';
        len += 1;
    }

    memcpy(buffer + len, b, b_len);
    len += b_len;

    buffer[len] = 0;

    return buffer;
}

static inline
char *SPrintf(const char *fmt, ...) {
    va_list va;

    va_start(va, fmt);
    int to_append = vsnprintf(NULL, 0, fmt, va);
    va_end(va);

    char *buff = malloc(to_append + 1);
    va_start(va, fmt);
    vsnprintf(buff, to_append + 1, fmt, va);
    va_end(va);

    return buff;
}

static inline
bool IsDirectory(const char *filename) {
#if defined(PLATFORM_WINDOWS)

    DWORD attr = GetFileAttributesA(filename);
    return (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;

#elif defined(PLATFORM_LINUX)

    struct stat file_stat;
    stat(filename, &file_stat);

    return !S_ISREG(file_stat.st_mode);

#endif
}

extern int g_num_errors;

static inline
void Error(const char *fmt, ...) {
    va_list va;

    printf("\x1b[1;31mError:\x1b[0m ");

    va_start(va, fmt);
    vprintf(fmt, va);
    va_end(va);

    printf("\n");

    g_num_errors += 1;
}

static inline
void ErrorExit(const char *fmt, ...) {
    va_list va;

    printf("\x1b[1;31mError:\x1b[0m ");

    va_start(va, fmt);
    vprintf(fmt, va);
    va_end(va);

    printf("\n");

    exit(1);
}

static inline
Array FileList(const char *dirname) {
    Array files = {0};

#if defined(PLATFORM_WINDOWS)
    char *wildcard = PathJoin(dirname, "*");

    WIN32_FIND_DATAA find_data = {0};
    HANDLE hfind = FindFirstFileA(wildcard, &find_data);
    if (hfind == INVALID_HANDLE_VALUE) {
        Error("Could not open directory '%s'", dirname);
        return files;
    }

    while (true) {
        char *filename = PathJoin(dirname, find_data.cFileName);
        ArrayPush(&files, filename);

        if (FindNextFileA(hfind, &find_data) == 0) {
            break;
        }
    }

    if (GetLastError() != ERROR_NO_MORE_FILES) {
        Error("There was an error when listing files in directory '%s' (%u)\n", dirname, GetLastError());
    }

    FindClose(hfind);

#elif defined(PLATFORM_LINUX)

    DIR *d = opendir(dirname);
    if (!d) {
        Error("Could not open directory '%s'", dirname);
        return files;
    }

    for (struct dirent *dir = readdir(d); dir != NULL; dir = readdir(d)) {
        char *filename = PathJoin(dirname, dir->d_name);
        ArrayPush(&files, filename);
    }

    closedir(d);

#endif

    return files;
}

typedef struct {
    int64_t capacity;
    int64_t count;
    char *data;
} StringBuilder;

static inline
void SBReserve(StringBuilder *builder, int64_t capacity) {
    if (capacity <= builder->capacity) {
        return;
    }

    builder->data = realloc(builder->data, capacity);
    builder->capacity = capacity;
}

static inline
void SBAppendByte(StringBuilder *builder, char byte) {
    if (builder->count >= builder->capacity) {
        SBReserve(builder, builder->capacity * 2 + 8);
    }

    builder->data[builder->count] = byte;
    builder->count += 1;
}

static inline
void SBAppendString(StringBuilder *builder, const char *str) {
    while (*str) {
        SBAppendByte(builder, *str);
        str += 1;
    }
}

static inline
void SBAppend(StringBuilder *builder, const char *fmt, ...) {
    va_list va;

    va_start(va, fmt);
    int to_append = vsnprintf(NULL, 0, fmt, va);
    va_end(va);

    if (builder->count + to_append + 1000 >= builder->capacity) {
        int64_t total = builder->count + to_append + 1000;
        if (total > builder->capacity * 2 + 8) {
            SBReserve(builder, total);
        } else {
            SBReserve(builder, builder->capacity * 2 + 8);
        }
    }

    va_start(va, fmt);
    vsnprintf(builder->data + builder->count, builder->capacity - builder->count, fmt, va);
    va_end(va);

    if (to_append > builder->capacity - builder->count) {
        builder->count = builder->capacity;
    } else {
        builder->count += to_append;
    }
}

static
char *SBBuild(StringBuilder *builder) {
    SBAppendByte(builder, 0);

    return builder->data;
}

static
int64_t WriteEntireFile(const char *filename, const void *buffer, int64_t size) {
    FILE *file = fopen(filename, "wb");
    if (!file) {
        return 0;
    }

    size_t written = fwrite(buffer, 1, size, file);
    fclose(file);

    return written;
}

#endif
